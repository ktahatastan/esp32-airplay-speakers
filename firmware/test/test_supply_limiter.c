/**
 * @file test_supply_limiter.c
 * @brief The adapter-budget stage, measured rather than asserted about.
 *
 * NOT A BUDGET. Every number here is invented for the arithmetic: the real
 * budget and window come from G1 step S7 on the adapter with all four
 * amplifiers into 4 ohm-class loads, and nothing in this file is a tuning
 * decision. What is under test is the LAW -- that an input whose mean square
 * exceeds the budget leaves with a mean square AT the budget, that an input
 * under it is untouched to the bit, and that the stage refuses what it cannot
 * honour -- and none of that depends on the numbers being the real ones.
 */
#include "hk_test.h"
#include "hk_supply_limiter.h"

#include <math.h>
#include <string.h>

#define FS 44100u
#define BLOCK 352u /* FRAME_SAMPLES in the vendored backend */

/**
 * Drive the stage with two steady sines, one per branch, for @p blocks blocks
 * and return the OUTPUT mean square measured over the last @p measure blocks.
 *
 * Block by block, the way hk_dsp drives it: block_begin then BLOCK steps. The
 * two sines are at unrelated frequencies so that w^2 + t^2 is not a single
 * tone the window might happen to null.
 */
static double run_two_tone(hk_supply_limiter_t *lim, float amp_w, float amp_t,
                           size_t blocks, size_t measure, float *min_gain)
{
    double phase_w = 0.0;
    double phase_t = 0.0;
    double out_sq = 0.0;
    size_t n = 0u;
    *min_gain = 1.0f;

    for (size_t b = 0; b < blocks; b++) {
        hk_supply_limiter_block_begin(lim, BLOCK);
        for (size_t i = 0; i < BLOCK; i++) {
            const float w = amp_w * (float)sin(phase_w);
            const float t = amp_t * (float)sin(phase_t);
            phase_w += 2.0 * M_PI * 997.0 / (double)FS;
            phase_t += 2.0 * M_PI * 3011.0 / (double)FS;
            const float g = hk_supply_limiter_step(lim, w, t);
            if (g < *min_gain) {
                *min_gain = g;
            }
            if (b + measure >= blocks) {
                const double ow = (double)(w * g);
                const double ot = (double)(t * g);
                out_sq += ow * ow + ot * ot;
                n++;
            }
        }
    }
    return out_sq / (double)n;
}

static void config_refusals(void)
{
    const hk_supply_limiter_config_t good = {0.5f, 50u, FS};
    HK_CHECK(hk_supply_limiter_config_valid(&good));
    HK_CHECK(!hk_supply_limiter_config_valid(NULL));

    hk_supply_limiter_config_t c = good;
    c.budget_sq = 0.0f;                      /* silent */
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.budget_sq = -0.1f;
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.budget_sq = NAN;                       /* unset */
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.budget_sq = INFINITY;
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.budget_sq = HK_SUPPLY_LIMITER_BUDGET_MAX; /* two full-scale branches: the edge is legal */
    HK_CHECK(hk_supply_limiter_config_valid(&c));
    c.budget_sq = HK_SUPPLY_LIMITER_BUDGET_MAX + 0.01f;
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.window_ms = 0u;                        /* a per-sample detector is a peak limiter */
    HK_CHECK(!hk_supply_limiter_config_valid(&c));
    c = good;
    c.sample_rate = 0u;
    HK_CHECK(!hk_supply_limiter_config_valid(&c));

    /* Init refuses the same things, and a refused stage is not ready. */
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &good));
    HK_CHECK(lim.ready);
    HK_CHECK(lim.gain == 1.0f);
    HK_CHECK(lim.mean_sq == 0.0f);
    HK_CHECK(lim.coeff > 0.0f && lim.coeff < 1.0f);

    c = good;
    c.window_ms = 0u;
    HK_CHECK(!hk_supply_limiter_init(&lim, &c));
    HK_CHECK(!lim.ready);
    HK_CHECK(!hk_supply_limiter_init(NULL, &good));
    HK_CHECK(!hk_supply_limiter_init(&lim, NULL));
    HK_CHECK(!lim.ready);

    /* Unready: unity, and nothing to do at block start. The caller is the one
     * that must refuse to run -- hk_dsp does, once per block. */
    hk_supply_limiter_block_begin(&lim, BLOCK);
    HK_CHECK(hk_supply_limiter_step(&lim, 0.9f, 0.9f) == 1.0f);
    HK_CHECK(hk_supply_limiter_step(NULL, 0.9f, 0.9f) == 1.0f);
    hk_supply_limiter_block_begin(NULL, BLOCK);
    hk_supply_limiter_reset(NULL);
}

/** Over budget by 2x: the output settles AT the budget, not near it. */
static void over_budget_converges_to_the_budget(void)
{
    /* Two sines whose combined mean square is 2 x budget: a sine's mean
     * square is A^2/2, so 0.4^2/2 + 0.2^2/2 = 0.10 against a budget of 0.05. */
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));

    /* Eight windows is 160 ms, about 20 blocks; run 24 and measure the last 4
     * (32 ms), which is more than a period of the slowest tone's beat. */
    float min_gain;
    const double out = run_two_tone(&lim, 0.4f, 0.2f, 24u, 4u, &min_gain);
    HK_CHECK(fabs(out - 0.05) < 0.05 * 0.05);

    /* The gain it settled at is the one the law predicts: sqrt(1/2). */
    HK_CHECK(fabsf(lim.gain - 0.70710678f) < 0.02f);
    HK_CHECK(min_gain > 0.6f);   /* no undershoot on the way down */

    /* And the detector reads the INPUT, not the output: it sits at 2 x budget
     * while the output sits at 1 x. That is what keeps the loop open. */
    HK_CHECK(fabsf(lim.mean_sq - 0.10f) < 0.01f);
}

/** Under budget: unity to the bit, on every sample, never "almost". */
static void under_budget_is_exactly_unity(void)
{
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));

    /* 0.2^2/2 + 0.1^2/2 = 0.025, half the budget. */
    double phase_w = 0.0;
    double phase_t = 0.0;
    size_t not_unity = 0u;
    for (size_t b = 0; b < 40u; b++) {
        hk_supply_limiter_block_begin(&lim, BLOCK);
        for (size_t i = 0; i < BLOCK; i++) {
            const float w = 0.2f * (float)sin(phase_w);
            const float t = 0.1f * (float)sin(phase_t);
            phase_w += 2.0 * M_PI * 997.0 / (double)FS;
            phase_t += 2.0 * M_PI * 3011.0 / (double)FS;
            if (hk_supply_limiter_step(&lim, w, t) != 1.0f) {
                not_unity++;
            }
        }
    }
    HK_CHECK_EQ_INT(not_unity, 0);
    HK_CHECK(lim.gain == 1.0f);
    HK_CHECK(lim.slew == 0.0f);
    HK_CHECK(fabsf(lim.mean_sq - 0.025f) < 0.003f);
}

/** The gain moves once per sample by a bounded step, never by a jump. */
static void gain_has_no_step_in_it(void)
{
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));

    /* Silence, then a loud two-tone from one block to the next: the worst
     * case for a stage that decided its gain by jumping. */
    float last = 1.0f;
    float worst_step = 0.0f;
    double phase_w = 0.0;
    double phase_t = 0.0;
    for (size_t b = 0; b < 40u; b++) {
        hk_supply_limiter_block_begin(&lim, BLOCK);
        for (size_t i = 0; i < BLOCK; i++) {
            const float w = (b < 4u) ? 0.0f : 0.8f * (float)sin(phase_w);
            const float t = (b < 4u) ? 0.0f : 0.4f * (float)sin(phase_t);
            phase_w += 2.0 * M_PI * 997.0 / (double)FS;
            phase_t += 2.0 * M_PI * 3011.0 / (double)FS;
            const float g = hk_supply_limiter_step(&lim, w, t);
            const float step = fabsf(g - last);
            if (step > worst_step) {
                worst_step = step;
            }
            last = g;
        }
    }
    /* A block's whole move is spread over its 352 frames, so no single step
     * can exceed 1/352 of the largest possible move, which is 1.0. */
    HK_CHECK(worst_step <= 1.0f / (float)BLOCK + 1e-6f);
    HK_CHECK(worst_step > 0.0f);   /* and it did move */
    HK_CHECK(lim.gain < 0.5f);     /* 0.8^2/2 + 0.4^2/2 = 0.40, 8x budget: sqrt(1/8) */
    HK_CHECK(fabsf(lim.gain - 0.35355f) < 0.02f);
}

/** A broken sample must not become a broken detector. */
static void nan_input_leaves_state_alone(void)
{
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));

    float min_gain;
    (void)run_two_tone(&lim, 0.4f, 0.2f, 24u, 4u, &min_gain);
    const hk_supply_limiter_t before = lim;
    HK_CHECK(before.gain < 1.0f);

    /* Mid-block, so the slew is live: the NaN frame must not advance it. */
    hk_supply_limiter_block_begin(&lim, BLOCK);
    const hk_supply_limiter_t armed = lim;
    HK_CHECK(hk_supply_limiter_step(&lim, NAN, 0.1f) == armed.gain);
    HK_CHECK(hk_supply_limiter_step(&lim, 0.1f, INFINITY) == armed.gain);
    HK_CHECK(hk_supply_limiter_step(&lim, -INFINITY, NAN) == armed.gain);
    HK_CHECK(lim.mean_sq == armed.mean_sq);
    HK_CHECK(lim.gain == armed.gain);
    HK_CHECK(lim.target == armed.target);

    /* An ordinary frame afterwards is processed as if nothing had happened. */
    (void)hk_supply_limiter_step(&lim, 0.1f, 0.1f);
    HK_CHECK(lim.mean_sq != armed.mean_sq);
}

/** Reset forgets the signal and nothing else. */
static void reset_restores_unity(void)
{
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));

    float min_gain;
    (void)run_two_tone(&lim, 0.4f, 0.2f, 24u, 4u, &min_gain);
    HK_CHECK(lim.gain < 1.0f);
    HK_CHECK(lim.mean_sq > 0.0f);

    const float coeff = lim.coeff;
    hk_supply_limiter_reset(&lim);
    HK_CHECK(lim.gain == 1.0f);
    HK_CHECK(lim.mean_sq == 0.0f);
    HK_CHECK(lim.target == 1.0f);
    HK_CHECK(lim.slew == 0.0f);
    HK_CHECK(lim.ready);
    HK_CHECK(lim.coeff == coeff);
    HK_CHECK(lim.config.budget_sq == c.budget_sq);

    /* The next quiet block is unity, exactly: nothing lingered. */
    hk_supply_limiter_block_begin(&lim, BLOCK);
    HK_CHECK(hk_supply_limiter_step(&lim, 0.01f, 0.01f) == 1.0f);
}

/** A block of zero frames has nothing to slew across and must not divide. */
static void zero_frames_is_harmless(void)
{
    const hk_supply_limiter_config_t c = {0.05f, 20u, FS};
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));
    lim.mean_sq = 0.2f; /* over budget, so a target below 1 is computed */
    hk_supply_limiter_block_begin(&lim, 0u);
    HK_CHECK(isfinite(lim.slew));
    HK_CHECK(lim.slew == 0.0f);
    HK_CHECK(lim.target < 1.0f);
    HK_CHECK(lim.gain == 1.0f);
}

/** The window is the detector's time constant, not just "some smoothing".
 *
 * The convergence cases above only look at the settled value, which a detector
 * with a window twice or half the profile's would also reach. G1 records the
 * window from the adapter's hold-up time, so the number in the profile has to
 * BE the time constant: after exactly one window the one-pole mean has closed
 * 1 - e^-1 of the distance, after two windows 1 - e^-2. Budget far above the
 * input so the gain stays at unity and only the detector is under test. */
static void window_is_the_time_constant(void)
{
    const hk_supply_limiter_config_t c = {0.5f, 20u, FS};
    const uint32_t window = 20u * FS / 1000u;
    const float power = 0.3f * 0.3f;
    hk_supply_limiter_t lim;
    HK_CHECK(hk_supply_limiter_init(&lim, &c));
    for (uint32_t i = 0; i < window; i++) {
        hk_supply_limiter_block_begin(&lim, 1u);
        (void)hk_supply_limiter_step(&lim, 0.3f, 0.0f);
    }
    HK_CHECK(fabsf(lim.mean_sq - power * (1.0f - expf(-1.0f))) < power * 0.02f);
    for (uint32_t i = 0; i < window; i++) {
        hk_supply_limiter_block_begin(&lim, 1u);
        (void)hk_supply_limiter_step(&lim, 0.3f, 0.0f);
    }
    HK_CHECK(fabsf(lim.mean_sq - power * (1.0f - expf(-2.0f))) < power * 0.02f);
    HK_CHECK(lim.gain == 1.0f);
}

void test_supply_limiter(void)
{
    config_refusals();
    over_budget_converges_to_the_budget();
    under_budget_is_exactly_unity();
    gain_has_no_step_in_it();
    nan_input_leaves_state_alone();
    reset_restores_unity();
    zero_frames_is_harmless();
    window_is_the_time_constant();
}
