#include "hk_supply_limiter.h"

#include <math.h>
#include <stddef.h>

bool hk_supply_limiter_config_valid(const hk_supply_limiter_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    /* NaN fails both comparisons, but isfinite says so by name: an unset
     * float is the likeliest way for this to arrive wrong. */
    if (!isfinite(config->budget_sq) || !(config->budget_sq > 0.0f) ||
        config->budget_sq > HK_SUPPLY_LIMITER_BUDGET_MAX) {
        return false;
    }
    /* A window under a millisecond is a per-sample detector, which is a peak
     * limiter with extra steps and not what the rail asks for. */
    if (config->window_ms == 0u) {
        return false;
    }
    if (config->sample_rate == 0u) {
        return false;
    }
    return true;
}

bool hk_supply_limiter_init(hk_supply_limiter_t *limiter,
                            const hk_supply_limiter_config_t *config)
{
    if (limiter == NULL) {
        return false;
    }
    limiter->ready = false;
    limiter->mean_sq = 0.0f;
    limiter->gain = 1.0f;
    limiter->target = 1.0f;
    limiter->slew = 0.0f;

    if (!hk_supply_limiter_config_valid(config)) {
        return false;
    }
    limiter->config = *config;

    /* One-pole mean over window_ms: after one window the detector has closed
     * about 63% of the distance to the input's mean square. The validator has
     * already ruled out a zero window and a zero rate, so samples is positive
     * and expf(-1 / samples) sits below 1 for any window the bench can name;
     * the [0, 1) check below is what refuses a window so long (minutes) that
     * the float rounds to exactly 1. */
    const float samples = ((float)config->window_ms / 1000.0f) *
                          (float)config->sample_rate;
    limiter->coeff = expf(-1.0f / samples);

    /* The detector recurrence only converges while the coefficient sits in
     * [0, 1). Checked once here so a change to the maths above fails at init
     * rather than being silently corrected a million times a second. */
    if (!(limiter->coeff >= 0.0f) || limiter->coeff >= 1.0f) {
        return false;
    }

    limiter->ready = true;
    return true;
}

void hk_supply_limiter_reset(hk_supply_limiter_t *limiter)
{
    if (limiter == NULL) {
        return;
    }
    limiter->mean_sq = 0.0f;
    limiter->gain = 1.0f;
    limiter->target = 1.0f;
    limiter->slew = 0.0f;
}

void hk_supply_limiter_block_begin(hk_supply_limiter_t *limiter, size_t frames)
{
    if (limiter == NULL || !limiter->ready) {
        return;
    }

    /* The gain that brings a signal with THIS mean square down to the
     * budget, applied to the input, gives an output whose mean square is the
     * budget: (g^2 * M = budget). Under budget the target is unity, not
     * "slightly above whatever it is now": this stage only ever reduces. */
    const float budget = limiter->config.budget_sq;
    const float mean_sq = limiter->mean_sq;
    limiter->target = (mean_sq <= budget) ? 1.0f : sqrtf(budget / mean_sq);

    /* Spread the move across the block so the gain has no step in it. With no
     * frames to spread it across there is nothing to slew -- the next block
     * with frames will recompute from a fresher detector anyway. */
    limiter->slew = (frames > 0u)
                        ? (limiter->target - limiter->gain) / (float)frames
                        : 0.0f;
}

float hk_supply_limiter_step(hk_supply_limiter_t *limiter, float woofer, float tweeter)
{
    if (limiter == NULL || !limiter->ready) {
        return 1.0f;
    }

    /* A broken sample must not become a broken detector that then holds the
     * gain down for a whole window. hk_limiter makes the same choice for the
     * same reason: the sample is already broken, and repairing it is the
     * output stage's job, not this one's. */
    const float power = woofer * woofer + tweeter * tweeter;
    if (!isfinite(power)) {
        return limiter->gain;
    }

    /* Feed-forward: the detector sees the INPUT, so the gain it produces
     * never feeds back into what it measures. */
    limiter->mean_sq += (1.0f - limiter->coeff) * (power - limiter->mean_sq);

    /* Towards the target and not past it. The slew was sized to land on the
     * target at the block's last frame; the two clamps are for float
     * rounding, so that "arrived" is exact and a block under budget with the
     * gain already at unity stays at exactly 1.0f. */
    if (limiter->slew > 0.0f) {
        limiter->gain += limiter->slew;
        if (limiter->gain > limiter->target) {
            limiter->gain = limiter->target;
        }
    } else if (limiter->slew < 0.0f) {
        limiter->gain += limiter->slew;
        if (limiter->gain < limiter->target) {
            limiter->gain = limiter->target;
        }
    }

    return limiter->gain;
}
