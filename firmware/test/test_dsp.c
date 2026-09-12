/**
 * @file test_dsp.c
 * @brief The signal path, measured rather than asserted about.
 *
 * Most of these tests drive real audio through hk_dsp_process() in blocks of
 * 352 frames -- the size the AirPlay backend actually hands it -- and measure
 * what comes out. A test that only checked coefficients would pass with the
 * stages wired in the wrong order.
 *
 * Nothing here proves an acoustic result. It proves the arithmetic does what
 * the header says; a driver, a room and a microphone are a different gate.
 */
#include "hk_test.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hk_dsp.h"
#include "hk_eq.h"
#include "hk_profile.h"
#include "hk_settings.h"

#define FS_HZ 44100.0f
#define BLOCK 352u /* FRAME_SAMPLES in the vendored backend */

/* ------------------------------------------------------------------ *
 * The orchestrator's provisional values.
 *
 * These are CONSERVATIVE CHOICES derived from two bench measurements, not
 * measurements themselves, and this file is not where they become official:
 * it is a test fixture, so the numbers exist here only to have something
 * concrete to measure. See docs/02-hardware/driver-measurements.md.
 *
 *   crossover 4000 Hz  The tweeter's Fs was NOT located: its impedance is flat
 *                      at 3.3-3.9 ohm from 500 to 3150 Hz with no peak above
 *                      about 5 ohm. If Fs is at or below 2000 Hz then 4 kHz is
 *                      at least twice it, and 4 kHz also coincides with the
 *                      10 uF C_SAFE capacitor's 3980 Hz corner, so the passive
 *                      backstop and the active crossover reinforce each other
 *                      instead of fighting. The cost is real: a 60 mm cone
 *                      beams above about 1.8 kHz, so this is high for sound
 *                      quality and is expected to come DOWN once Fs is
 *                      measured.
 *   subsonic    70 Hz  Below the woofer's measured resonance -- its impedance
 *                      peaks at 11.6 ohm at 100 Hz falling to 4.8 ohm by
 *                      300 Hz, so Fs is at or below 100 Hz. Under Fs a driver
 *                      makes excursion and no output.
 *   tweeter gain -3 dB A 25 mm dome is usually more sensitive than a small
 *                      cone. There is no sensitivity measurement, so the error
 *                      is deliberately left on the side of less tweeter.
 *
 * The bench build compiles a different placeholder (2800 Hz, 55 Hz, in the
 * output backend); this fixture is deliberately at the C_SAFE corner and is
 * not the record's placeholder. Neither number is a measurement.
 * ------------------------------------------------------------------ */
#define PROVISIONAL_CROSSOVER_HZ 4000.0f
#define PROVISIONAL_SUBSONIC_HZ  70.0f
#define PROVISIONAL_TWEETER_GAIN 0.70794578f /* -3.0 dB */

/* The 24 V adapter (ADR-0020). The fixture is referenced to the same supply it
 * is built at, so the ceilings pass through unscaled and the tests below
 * measure the filters, not the supply arithmetic (test_profile.c does that). */
#define SUPPLY_MV 24000.0f

static hk_profile_t fixture_profile(void)
{
    hk_profile_t p;
    memset(&p, 0, sizeof(p));
    p.schema = HK_PROFILE_SCHEMA;
    p.measured_yyyymmdd = 20260908u;
    /* A profile must name what it was derived from, and this one is honest
     * about being a fixture rather than borrowing a real record's name. */
    strncpy(p.source, "test-fixture", sizeof(p.source) - 1u);
    /* The two DC resistances the operator read on 2026-09-08
     * (docs/02-hardware/driver-measurements.md): woofer 4.0, tweeter 3.5.
     * The firmware carried 3.7 for the tweeter before today; the record is
     * the source, and the operator should confirm the reading. Carried, never
     * computed with, so the tests do not depend on either value. */
    p.woofer_dcr_ohm = 4.0f;
    p.tweeter_dcr_ohm = 3.5f;
    p.woofer_hpf_hz = PROVISIONAL_SUBSONIC_HZ;
    p.crossover_hz = PROVISIONAL_CROSSOVER_HZ;
    p.woofer_gain = 1.0f;
    p.tweeter_gain = PROVISIONAL_TWEETER_GAIN;
    p.reference_supply_mv = SUPPLY_MV;
    p.woofer_ceiling = 1.0f;
    p.tweeter_ceiling = 1.0f;
    p.woofer_release_ms = 100u;
    p.woofer_hold_ms = 10u;
    /* Schema 2. Same timing on both branches here so the existing limiter
     * tests measure what they measured before; no delay, no inversion, and a
     * budget at the validator's maximum so the supply stage cannot engage
     * unless a test lowers it on purpose. The window is a fixture value. */
    p.tweeter_release_ms = 100u;
    p.tweeter_hold_ms = 10u;
    p.woofer_delay_samples = 0u;
    p.tweeter_delay_samples = 0u;
    p.tweeter_polarity = 0u;
    p.supply_budget_sq = HK_PROFILE_SUPPLY_BUDGET_MAX;
    p.supply_window_ms = 100u;
    p.amp_gain_db = 0u;
    return p;
}

/** Same filters, both branches at unity: the crossover without the level trim. */
static hk_profile_t fixture_profile_equal_gains(void)
{
    hk_profile_t p = fixture_profile();
    p.tweeter_gain = 1.0f;
    return p;
}

static bool build(hk_dsp_t *dsp, const hk_profile_t *profile,
                  const hk_eq_settings_t *eq)
{
    hk_profile_chain_t chain;
    if (hk_profile_build(profile, FS_HZ, SUPPLY_MV, &chain) != HK_PROFILE_OK) {
        return false;
    }
    return hk_dsp_init(dsp, &chain, eq, FS_HZ);
}

/**
 * Drive a steady sine through the path and report each branch's level in dB
 * relative to the input.
 *
 * The same sample goes into both input slots, so the mono downmix is the
 * identity and what is measured is the chain rather than the downmix.
 */
static void measure(hk_dsp_t *dsp, float hz, float amplitude,
                    float *woofer_db, float *tweeter_db, float *sum_db)
{
    const size_t settle_blocks = 32u;  /* ~11k frames; the 70 Hz section needs it */
    const size_t count_blocks = 64u;
    int16_t buf[BLOCK * 2u];

    const double step = 2.0 * M_PI * (double)hz / (double)FS_HZ;
    double phase = 0.0;
    double w_sum = 0.0;
    double t_sum = 0.0;
    double s_sum = 0.0;
    size_t n = 0u;

    hk_dsp_reset(dsp);

    for (size_t b = 0; b < settle_blocks + count_blocks; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            const double s = (double)amplitude * sin(phase);
            phase += step;
            const int16_t v = (int16_t)(s * 32767.0);
            buf[2u * i] = v;
            buf[2u * i + 1u] = v;
        }
        (void)hk_dsp_process(dsp, buf, BLOCK);
        if (b >= settle_blocks) {
            for (size_t i = 0; i < BLOCK; i++) {
                const double w = (double)buf[2u * i] / 32768.0;
                const double t = (double)buf[2u * i + 1u] / 32768.0;
                w_sum += w * w;
                t_sum += t * t;
                /* The branches are added SAMPLE BY SAMPLE, not in power.
                 *
                 * "The low and high branches sum flat" is a statement about a
                 * COHERENT sum: LR4's two branches are in phase, so what the
                 * two drivers put into the same air is w + t. A power sum
                 * (w^2 + t^2) is what uncorrelated sources do, and it reads
                 * -3 dB at the crossover where the real answer is 0 dB -- it
                 * would fail a correct crossover and pass an inverted one,
                 * since squaring throws away the sign that a polarity mistake
                 * lives in. */
                s_sum += (w + t) * (w + t);
                n++;
            }
        }
    }

    const double in_rms = (double)amplitude / sqrt(2.0);
    *woofer_db = (float)(20.0 * log10((sqrt(w_sum / (double)n) / in_rms) + 1e-30));
    *tweeter_db = (float)(20.0 * log10((sqrt(t_sum / (double)n) / in_rms) + 1e-30));
    *sum_db = (float)(20.0 * log10((sqrt(s_sum / (double)n) / in_rms) + 1e-30));
}

/* ------------------------------------------------------------------ */

static void eq_defaults_are_flat(void)
{
    const hk_eq_settings_t defaults = hk_eq_defaults();
    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        HK_CHECK(defaults.band[b].gain_db == 0.0f);
        HK_CHECK(defaults.band[b].hz > 0.0f);
        HK_CHECK(defaults.band[b].q > 0.0f);
    }
    HK_CHECK(defaults.trim_db == 0.0f);

    hk_eq_t eq;
    HK_CHECK(hk_eq_build(&eq, &defaults, FS_HZ));
    /* Not "approximately flat": no band is designed at all, so the stage is
     * bit-exact identity and costs nothing. */
    HK_CHECK_EQ_INT(hk_eq_active_bands(&eq), 0);
    HK_CHECK(eq.rejected == 0u);
    HK_CHECK(eq.trim == 1.0f);
    HK_CHECK(hk_eq_process_one(&eq, 0.123456f) == 0.123456f);
    HK_CHECK(hk_eq_process_one(&eq, -0.9f) == -0.9f);
}

static void eq_settings_table_is_spliceable(void)
{
    /* The rows obey every rule the project's own table obeys. */
    HK_CHECK(hk_settings_table_check(hk_eq_settings_table));
    HK_CHECK_EQ_INT(hk_eq_settings_count(), 10);

    /* And no key collides with one already in use, so appending them to
     * hk_settings_table[] is provably safe before anyone does it. */
    for (size_t i = 0; hk_eq_settings_table[i].key != NULL; i++) {
        HK_CHECK(hk_settings_find(hk_eq_settings_table[i].key) == NULL);
        HK_CHECK(strlen(hk_eq_settings_table[i].key) <= HK_SETTINGS_KEY_MAX);
    }
}

/** A stub store: returns whatever the test put in it. */
typedef struct {
    const char *key;
    uint32_t    value;
} stub_row_t;

typedef struct {
    const stub_row_t *rows;
    size_t            count;
} stub_store_t;

static bool stub_read(const char *key, uint32_t *out, void *ctx)
{
    const stub_store_t *store = (const stub_store_t *)ctx;
    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->rows[i].key, key) == 0) {
            *out = store->rows[i].value;
            return true;
        }
    }
    return false;
}

static void eq_settings_decode(void)
{
    hk_eq_settings_t s;

    /* Nothing stored: defaults, and the caller is told they were defaults. */
    stub_store_t empty = {NULL, 0u};
    HK_CHECK(!hk_eq_settings_load(&s, stub_read, &empty));
    HK_CHECK(s.band[0].gain_db == 0.0f);

    /* Stored and in range: decoded. 180 is (180-120)/10 = +6.0 dB. */
    static const stub_row_t good[] = {
        {"eq_lo_hz", 120u}, {"eq_lo_db", 180u}, {"eq_lo_q", 71u},
        {"eq_mid_hz", 3000u}, {"eq_mid_db", 60u}, {"eq_mid_q", 200u},
        {"eq_hi_hz", 9000u}, {"eq_hi_db", 120u}, {"eq_hi_q", 71u},
        {"eq_trim", 30u},
    };
    stub_store_t stored = {good, sizeof(good) / sizeof(good[0])};
    HK_CHECK(hk_eq_settings_load(&s, stub_read, &stored));
    HK_CHECK(fabsf(s.band[0].hz - 120.0f) < 0.001f);
    HK_CHECK(fabsf(s.band[0].gain_db - 6.0f) < 0.001f);
    HK_CHECK(fabsf(s.band[1].gain_db + 6.0f) < 0.001f);  /* 60 -> -6.0 dB */
    HK_CHECK(fabsf(s.band[1].q - 2.0f) < 0.001f);
    HK_CHECK(s.band[2].gain_db == 0.0f);                 /* 120 -> flat */
    HK_CHECK(fabsf(s.trim_db + 3.0f) < 0.001f);          /* 30 -> -3.0 dB cut */

    /* Out of range is not a preference: it falls back rather than clamping. */
    static const stub_row_t bad[] = {
        {"eq_lo_db", 9999u}, {"eq_lo_hz", 0u}, {"eq_trim", 5000u},
    };
    stub_store_t corrupt = {bad, sizeof(bad) / sizeof(bad[0])};
    HK_CHECK(!hk_eq_settings_load(&s, stub_read, &corrupt));
    HK_CHECK(s.band[0].gain_db == 0.0f);
    HK_CHECK(fabsf(s.band[0].hz - 100.0f) < 0.001f);
    HK_CHECK(s.trim_db == 0.0f);

    /* No reader at all is the same as an empty store, never uninitialised. */
    HK_CHECK(!hk_eq_settings_load(&s, NULL, NULL));
    HK_CHECK(s.band[1].gain_db == 0.0f);
}

static void eq_design_refuses_the_impossible(void)
{
    hk_biquad_coeffs_t c;
    /* At or above Nyquist there is no filter to design. */
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 22050.0f, FS_HZ, 1.0f, 6.0f));
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 30000.0f, FS_HZ, 1.0f, 6.0f));
    /* Below the ratio single precision can hold stably. */
    HK_CHECK(!hk_eq_design(&c, HK_EQ_LOW_SHELF, 1.0f, FS_HZ, 0.71f, 6.0f));
    /* Not filters. */
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 1000.0f, FS_HZ, 0.0f, 6.0f));
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 1000.0f, 0.0f, 1.0f, 6.0f));
    /* Beyond the authority a tone control is given. */
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 1000.0f, FS_HZ, 1.0f, 24.0f));
    HK_CHECK(!hk_eq_design(&c, HK_EQ_PEAKING, 1000.0f, FS_HZ, 1.0f, -24.0f));
    /* And the ones that should work, do. */
    HK_CHECK(hk_eq_design(&c, HK_EQ_PEAKING, 1000.0f, FS_HZ, 1.0f, 6.0f));
    HK_CHECK(hk_biquad_stable(&c));
    HK_CHECK(hk_eq_design(&c, HK_EQ_LOW_SHELF, 100.0f, FS_HZ, 0.71f, -6.0f));
    HK_CHECK(hk_biquad_stable(&c));
    HK_CHECK(hk_eq_design(&c, HK_EQ_HIGH_SHELF, 8000.0f, FS_HZ, 0.71f, 6.0f));
    HK_CHECK(hk_biquad_stable(&c));
}

/** A band that will not design is bypassed; the speaker keeps playing. */
static void eq_rejects_one_band_not_the_chain(void)
{
    hk_eq_settings_t s = hk_eq_defaults();
    s.band[1].hz = 40000.0f;  /* above Nyquist */
    s.band[1].gain_db = 6.0f;
    s.band[2].gain_db = 3.0f; /* fine */

    hk_eq_t eq;
    HK_CHECK(hk_eq_build(&eq, &s, FS_HZ));
    HK_CHECK(eq.rejected == (1u << 1));
    HK_CHECK_EQ_INT(hk_eq_active_bands(&eq), 1);
}

/* ------------------------------------------------------------------ */

static void dsp_refuses_without_a_profile(void)
{
    hk_dsp_t dsp;
    HK_CHECK(!hk_dsp_init(&dsp, NULL, NULL, FS_HZ));
    HK_CHECK(!hk_dsp_ready(&dsp));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_NO_CHAIN);
    HK_CHECK_EQ_STR(hk_dsp_refusal_name(hk_dsp_refusal(&dsp)), "uncalibrated");

    /* The heart of the refusal: loud full-band programme in, silence out and
     * a false. NOT passthrough into an unmeasured tweeter. */
    int16_t buf[BLOCK * 2u];
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = (int16_t)((i % 2u) ? 30000 : -30000);
    }
    HK_CHECK(!hk_dsp_process(&dsp, buf, BLOCK));

    int16_t peak = 0;
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        const int16_t magnitude = (int16_t)((buf[i] < 0) ? -buf[i] : buf[i]);
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    HK_CHECK_EQ_INT(peak, 0);
    HK_CHECK_EQ_INT(hk_dsp_biquads_per_frame(&dsp), 0);
}

static void dsp_refuses_a_chain_from_another_rate(void)
{
    const hk_profile_t p = fixture_profile();
    hk_profile_chain_t chain;
    HK_CHECK_EQ_INT(hk_profile_build(&p, 48000.0f, SUPPLY_MV, &chain), HK_PROFILE_OK);

    /* The filters would work; the limiters' release times would not, because
     * they were converted to sample counts at 48 kHz. */
    hk_dsp_t dsp;
    HK_CHECK(!hk_dsp_init(&dsp, &chain, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_RATE);
}

static void dsp_refuses_a_corrupt_chain(void)
{
    const hk_profile_t p = fixture_profile();
    hk_profile_chain_t chain;
    HK_CHECK_EQ_INT(hk_profile_build(&p, FS_HZ, SUPPLY_MV, &chain), HK_PROFILE_OK);

    hk_dsp_t dsp;

    /* A chain is a plain struct and could arrive from anywhere. Poles on or
     * outside the unit circle are a ringing filter, not a crossover. */
    hk_profile_chain_t unstable = chain;
    unstable.tweeter_high.section[1].a2 = 1.5f;
    HK_CHECK(!hk_dsp_init(&dsp, &unstable, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_FILTER);

    hk_profile_chain_t hot = chain;
    hot.tweeter_gain = 4.0f;
    HK_CHECK(!hk_dsp_init(&dsp, &hot, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_GAIN);

    hk_profile_chain_t no_release = chain;
    no_release.tweeter_limit.release_ms = 0u;
    HK_CHECK(!hk_dsp_init(&dsp, &no_release, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_LIMITER);

    /* A NaN in a NUMERATOR leaves the poles where they were, so the pole test
     * alone passed it and the section played silence with no refusal. Now it
     * is refused by name, in every protective cascade. */
    hk_profile_chain_t nan_b0 = chain;
    nan_b0.woofer_hpf.section[0].b0 = NAN;
    HK_CHECK(!hk_dsp_init(&dsp, &nan_b0, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_FILTER);
    nan_b0 = chain;
    nan_b0.woofer_hpf.section[1].b2 = INFINITY;
    HK_CHECK(!hk_dsp_init(&dsp, &nan_b0, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_FILTER);
    nan_b0 = chain;
    nan_b0.woofer_low.section[1].b1 = NAN;
    HK_CHECK(!hk_dsp_init(&dsp, &nan_b0, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_FILTER);

    /* A polarity that is not a polarity is a gain in disguise. */
    hk_profile_chain_t half_sign = chain;
    half_sign.tweeter_sign = 0.5f;
    HK_CHECK(!hk_dsp_init(&dsp, &half_sign, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_GAIN);
    half_sign.tweeter_sign = -1.0f;
    HK_CHECK(hk_dsp_init(&dsp, &half_sign, NULL, FS_HZ));

    /* Delay: over the bound, or both branches -- defence in depth over the
     * profile validator, because a chain could have come from anywhere. */
    hk_profile_chain_t far = chain;
    far.tweeter_delay_samples = HK_PROFILE_DELAY_MAX_SAMPLES + 1u;
    HK_CHECK(!hk_dsp_init(&dsp, &far, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_DELAY);
    HK_CHECK_EQ_STR(hk_dsp_refusal_name(hk_dsp_refusal(&dsp)), "delay");
    far = chain;
    far.woofer_delay_samples = 3u;
    far.tweeter_delay_samples = 3u;
    HK_CHECK(!hk_dsp_init(&dsp, &far, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_DELAY);
    far = chain;
    far.woofer_delay_samples = HK_PROFILE_DELAY_MAX_SAMPLES; /* the edge is legal */
    HK_CHECK(hk_dsp_init(&dsp, &far, NULL, FS_HZ));

    /* The supply stage refuses its configuration and the chain with it. */
    hk_profile_chain_t no_budget = chain;
    no_budget.supply_limit.budget_sq = 0.0f;
    HK_CHECK(!hk_dsp_init(&dsp, &no_budget, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_SUPPLY);
    HK_CHECK_EQ_STR(hk_dsp_refusal_name(hk_dsp_refusal(&dsp)), "supply");
    no_budget = chain;
    no_budget.supply_limit.window_ms = 0u;
    HK_CHECK(!hk_dsp_init(&dsp, &no_budget, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_SUPPLY);

    /* And a supply window converted at another rate is a rate mismatch, the
     * same as a limiter's release time would be. */
    hk_profile_chain_t other_rate = chain;
    other_rate.supply_limit.sample_rate = 48000u;
    HK_CHECK(!hk_dsp_init(&dsp, &other_rate, NULL, FS_HZ));
    HK_CHECK_EQ_INT(hk_dsp_refusal(&dsp), HK_DSP_BAD_RATE);
}

/**
 * The limiter is not optional and cannot be stepped over.
 *
 * The inner loop calls hk_limiter_step() rather than hk_limiter_process(), and
 * step() returns a gain of 1.0 when a limiter was never configured -- full
 * level, straight through. This is the check that the block-level guard
 * catches that instead.
 */
static void dsp_limiter_cannot_be_bypassed(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));
    HK_CHECK(hk_dsp_ready(&dsp));

    int16_t buf[BLOCK * 2u];
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = 24000;
    }
    HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));

    dsp.tweeter_limit.ready = false;
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = 24000;
    }
    HK_CHECK(!hk_dsp_process(&dsp, buf, BLOCK));
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        HK_CHECK_EQ_INT(buf[i], 0);
    }

    /* The supply stage is guarded the same way: its step function returns
     * unity when unready, which is the passthrough the block guard exists to
     * catch. */
    HK_CHECK(build(&dsp, &p, NULL));
    dsp.supply_limit.ready = false;
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = 24000;
    }
    HK_CHECK(!hk_dsp_process(&dsp, buf, BLOCK));
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        HK_CHECK_EQ_INT(buf[i], 0);
    }
}

/** Whatever the EQ asks for, the ceiling is what leaves. */
static void dsp_ceiling_holds_under_a_boost(void)
{
    hk_profile_t p = fixture_profile();
    p.woofer_ceiling = 0.25f;
    p.tweeter_ceiling = 0.10f;

    hk_eq_settings_t eq = hk_eq_defaults();
    eq.band[0].gain_db = 12.0f;  /* everything the owner is allowed to add */
    eq.band[1].gain_db = 12.0f;
    eq.band[2].gain_db = 12.0f;

    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, &eq));
    HK_CHECK_EQ_INT(hk_dsp_biquads_per_frame(&dsp), 9);

    /* Full-scale programme across the band, plus every boost available. */
    const int16_t woofer_max = (int16_t)(0.25f * 32767.0f + 2.0f);
    const int16_t tweeter_max = (int16_t)(0.10f * 32767.0f + 2.0f);
    int16_t buf[BLOCK * 2u];
    double phase = 0.0;

    for (size_t b = 0; b < 200u; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            /* A sweep, so both branches are excited over the whole run. */
            const double hz = 40.0 + 15000.0 * ((double)b / 200.0);
            phase += 2.0 * M_PI * hz / (double)FS_HZ;
            const int16_t v = (int16_t)(32000.0 * sin(phase));
            buf[2u * i] = v;
            buf[2u * i + 1u] = v;
        }
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
        for (size_t i = 0; i < BLOCK; i++) {
            const int16_t w = (int16_t)((buf[2u * i] < 0) ? -buf[2u * i] : buf[2u * i]);
            const int16_t t = (int16_t)((buf[2u * i + 1u] < 0) ? -buf[2u * i + 1u]
                                                              : buf[2u * i + 1u]);
            HK_CHECK(w <= woofer_max);
            HK_CHECK(t <= tweeter_max);
        }
    }

    /* And with the supply stage ACTIVE -- a budget small enough that it is
     * engaged for most of the sweep -- the ceilings still hold, because the
     * peak limiters run after it and a gain in (0, 1] cannot lift a sample. */
    p.supply_budget_sq = 0.005f;
    p.supply_window_ms = 10u;
    HK_CHECK(build(&dsp, &p, &eq));
    phase = 0.0;
    size_t engaged_blocks = 0u;
    for (size_t b = 0; b < 200u; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            const double hz = 40.0 + 15000.0 * ((double)b / 200.0);
            phase += 2.0 * M_PI * hz / (double)FS_HZ;
            const int16_t v = (int16_t)(32000.0 * sin(phase));
            buf[2u * i] = v;
            buf[2u * i + 1u] = v;
        }
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
        if (dsp.supply_limit.gain < 1.0f) {
            engaged_blocks++;
        }
        for (size_t i = 0; i < BLOCK; i++) {
            const int16_t w = (int16_t)((buf[2u * i] < 0) ? -buf[2u * i] : buf[2u * i]);
            const int16_t t = (int16_t)((buf[2u * i + 1u] < 0) ? -buf[2u * i + 1u]
                                                              : buf[2u * i + 1u]);
            HK_CHECK(w <= woofer_max);
            HK_CHECK(t <= tweeter_max);
        }
    }
    HK_CHECK(engaged_blocks > 100u);
}

/** Tonal settings cannot reach a protective number. Byte for byte. */
static void dsp_eq_cannot_touch_the_chain(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    hk_profile_chain_t before = dsp.chain;
    const hk_limiter_config_t woofer_before = dsp.woofer_limit.config;
    const hk_limiter_config_t tweeter_before = dsp.tweeter_limit.config;

    hk_eq_settings_t wild = hk_eq_defaults();
    wild.band[0].gain_db = -12.0f;
    wild.band[1].gain_db = 12.0f;
    wild.band[1].hz = PROVISIONAL_CROSSOVER_HZ; /* aimed straight at the corner */
    wild.band[2].gain_db = 12.0f;
    wild.trim_db = -12.0f;
    HK_CHECK(hk_dsp_set_eq(&dsp, &wild));

    HK_CHECK(memcmp(&before, &dsp.chain, sizeof(before)) == 0);
    HK_CHECK(memcmp(&woofer_before, &dsp.woofer_limit.config, sizeof(woofer_before)) == 0);
    HK_CHECK(memcmp(&tweeter_before, &dsp.tweeter_limit.config, sizeof(tweeter_before)) == 0);
    HK_CHECK(hk_dsp_ready(&dsp));

    /* And a rejected band still cannot take the path down. */
    hk_eq_settings_t impossible = hk_eq_defaults();
    impossible.band[0].hz = 1.0f;
    impossible.band[0].gain_db = 6.0f;
    HK_CHECK(!hk_dsp_set_eq(&dsp, &impossible));
    HK_CHECK(hk_dsp_ready(&dsp));
    HK_CHECK(memcmp(&before, &dsp.chain, sizeof(before)) == 0);
}

/** Stereo in, two driver branches out -- not two halves of the mix. */
static void dsp_maps_channels_to_drivers(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    /* A 400 Hz tone present ONLY in the input's left channel must still reach
     * the woofer branch at roughly half amplitude -- the mono downmix -- and
     * must NOT be the reason the tweeter branch is quiet. */
    int16_t buf[BLOCK * 2u];
    double phase = 0.0;
    double w_sum = 0.0;
    double t_sum = 0.0;
    size_t n = 0u;

    for (size_t b = 0; b < 96u; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            phase += 2.0 * M_PI * 400.0 / (double)FS_HZ;
            buf[2u * i] = (int16_t)(16000.0 * sin(phase));
            buf[2u * i + 1u] = 0;
        }
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
        if (b >= 32u) {
            for (size_t i = 0; i < BLOCK; i++) {
                const double w = (double)buf[2u * i] / 32768.0;
                const double t = (double)buf[2u * i + 1u] / 32768.0;
                w_sum += w * w;
                t_sum += t * t;
                n++;
            }
        }
    }

    const double w_rms = sqrt(w_sum / (double)n);
    const double t_rms = sqrt(t_sum / (double)n);
    /* Left-only input, amplitude 16000/32768, halved by the downmix, so about
     * 0.244 peak and 0.173 RMS through a passband. */
    HK_CHECK(w_rms > 0.15 && w_rms < 0.19);
    /* 400 Hz is four octaves below a 4 kHz LR4 high branch: gone. */
    HK_CHECK(t_rms < 0.0005);
}

/** The corners are where the profile asked, and the branches sum flat. */
static void dsp_response_matches_the_profile(void)
{
    const hk_profile_t p = fixture_profile_equal_gains();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    float w, t, s;

    /* LR4: each branch 6 dB down at the crossover, and in phase. */
    measure(&dsp, PROVISIONAL_CROSSOVER_HZ, 0.5f, &w, &t, &s);
    HK_CHECK(fabsf(w + 6.0f) < 0.4f);
    HK_CHECK(fabsf(t + 6.0f) < 0.4f);
    /* Two branches 6 dB down adding back to unity is the whole reason LR4 was
     * chosen over a Butterworth pair. It is also the check that catches an
     * inverted branch: invert one and this reads a deep null instead of 0 dB. */
    HK_CHECK(fabsf(s) < 0.4f);

    /* And flat across the decade around the corner, above the subsonic
     * filter's reach. */
    static const float sum_points[] = {500.0f, 1000.0f, 2000.0f, 3000.0f,
                                       5500.0f, 8000.0f, 12000.0f};
    for (size_t i = 0; i < sizeof(sum_points) / sizeof(sum_points[0]); i++) {
        measure(&dsp, sum_points[i], 0.5f, &w, &t, &s);
        HK_CHECK(fabsf(s) < 0.4f);
    }

    /* The subsonic filter is where the profile put it: fourth-order
     * Butterworth, so 3 dB down at its own corner -- the point the stored
     * field names, unchanged from the second-order section it replaced --
     * and about 24 dB down an octave below, where the old section left 12.
     * Two octaves below is the slope check: another 24 dB. */
    measure(&dsp, PROVISIONAL_SUBSONIC_HZ, 0.5f, &w, &t, &s);
    HK_CHECK(fabsf(w + 3.0f) < 0.6f);
    float one_octave;
    measure(&dsp, PROVISIONAL_SUBSONIC_HZ / 2.0f, 0.5f, &one_octave, &t, &s);
    HK_CHECK(one_octave < -22.0f && one_octave > -26.5f);
    float two_octaves;
    measure(&dsp, PROVISIONAL_SUBSONIC_HZ / 4.0f, 0.5f, &two_octaves, &t, &s);
    HK_CHECK(fabsf((one_octave - two_octaves) - 24.0f) < 1.5f);
    /* And flat two octaves above it, where a 4 kHz LR4 low branch is also
     * still flat: the subsonic filter is maximally flat, not a shelf. */
    measure(&dsp, PROVISIONAL_SUBSONIC_HZ * 4.0f, 0.5f, &w, &t, &s);
    HK_CHECK(fabsf(w) < 0.4f);

    /* And the tweeter branch really is 24 dB/octave: an octave below 4 kHz. */
    measure(&dsp, PROVISIONAL_CROSSOVER_HZ / 2.0f, 0.5f, &w, &t, &s);
    HK_CHECK(t < -22.0f);
    HK_CHECK(fabsf(w) < 0.6f);
}

/** The deliberate 3 dB is a level choice, not a crossover property. */
static void dsp_applies_the_branch_gains(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    float w, t, s;
    measure(&dsp, 12000.0f, 0.5f, &w, &t, &s);
    /* Well above the crossover the high branch is flat, so what is left is the
     * tweeter gain: -3.0 dB. */
    HK_CHECK(fabsf(t + 3.0f) < 0.4f);
    measure(&dsp, 500.0f, 0.5f, &w, &t, &s);
    HK_CHECK(fabsf(w) < 0.4f);
}

/** A band asked for 6 dB delivers 6 dB, and only where it was aimed. */
static void dsp_eq_does_what_it_says(void)
{
    const hk_profile_t p = fixture_profile_equal_gains();

    hk_eq_settings_t eq = hk_eq_defaults();
    eq.band[1].hz = 1000.0f;
    eq.band[1].gain_db = 6.0f;
    eq.band[1].q = 2.0f;

    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, &eq));
    HK_CHECK_EQ_INT(hk_dsp_biquads_per_frame(&dsp), 7);

    float w, t, s;
    measure(&dsp, 1000.0f, 0.25f, &w, &t, &s);
    HK_CHECK(fabsf(w - 6.0f) < 0.3f);

    /* A Q of 2 is about half an octave either side, so two octaves away the
     * band must be doing essentially nothing. This is what catches a shelf
     * accidentally wired where a peak belongs. */
    measure(&dsp, 250.0f, 0.25f, &w, &t, &s);
    HK_CHECK(fabsf(w) < 0.7f);

    /* The trim cuts, broadband, and does not tilt anything. */
    hk_eq_settings_t trimmed = hk_eq_defaults();
    trimmed.trim_db = -6.0f;
    HK_CHECK(hk_dsp_set_eq(&dsp, &trimmed));
    HK_CHECK_EQ_INT(hk_dsp_biquads_per_frame(&dsp), 6);
    measure(&dsp, 500.0f, 0.25f, &w, &t, &s);
    HK_CHECK(fabsf(w + 6.0f) < 0.3f);
    measure(&dsp, 12000.0f, 0.25f, &w, &t, &s);
    HK_CHECK(fabsf(t + 6.0f) < 0.3f);
}

/** Reset clears filter memory, so a flush does not leak the old stream. */
static void dsp_reset_clears_state(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    int16_t loud[BLOCK * 2u];
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        loud[i] = 30000;
    }
    HK_CHECK(hk_dsp_process(&dsp, loud, BLOCK));

    hk_dsp_reset(&dsp);
    HK_CHECK(dsp.hpf_state.section[0].z1 == 0.0f && dsp.hpf_state.section[0].z2 == 0.0f);
    HK_CHECK(dsp.hpf_state.section[1].z1 == 0.0f && dsp.hpf_state.section[1].z2 == 0.0f);
    HK_CHECK(dsp.woofer_limit.gain == 1.0f);
    HK_CHECK(dsp.woofer_limit.ready);
    HK_CHECK(dsp.supply_limit.gain == 1.0f);
    HK_CHECK(dsp.supply_limit.mean_sq == 0.0f);
    HK_CHECK(dsp.supply_limit.ready);

    int16_t quiet[BLOCK * 2u];
    memset(quiet, 0, sizeof(quiet));
    HK_CHECK(hk_dsp_process(&dsp, quiet, BLOCK));
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        HK_CHECK_EQ_INT(quiet[i], 0);
    }
}

/** One block of a steady 120 Hz tone, continuing @p phase. */
static void tone_block(int16_t *buf, double *phase)
{
    for (size_t i = 0; i < BLOCK; i++) {
        const int16_t v = (int16_t)(8000.0 * sin(*phase));
        *phase += 2.0 * M_PI * 120.0 / (double)FS_HZ;
        buf[2u * i] = v;
        buf[2u * i + 1u] = v;
    }
}

/**
 * Editing one tone control must not throw the other two off course.
 *
 * hk_eq_build() zeroes the whole hk_eq_t, filter memory included. That is what
 * makes it safe to call on uninitialised storage -- hk_dsp_init() relies on it
 * -- and wrong on a path that is playing, because a running section's memory is
 * the tail of the music.
 *
 * MEASURED AS A DEVIATION, NOT AS A STEP. The gap between two adjacent output
 * samples is mostly the waveform's own slope and says almost nothing; the
 * question is how far the output departs from the path that had the new
 * settings all along. So two instances run the same tone, one built with the
 * final settings from the start and one switched to them mid-stream, and the
 * difference is the whole artefact. Before the carry-over in hk_dsp_set_eq()
 * that peaked at 9029 LSB -- 27.6% of full scale, three quarters of the
 * signal's own amplitude, on a band the settings did not even mention.
 */
static void dsp_set_eq_does_not_click_the_other_bands(void)
{
    hk_profile_t p = fixture_profile();
    p.woofer_ceiling = 1.0f; /* keep the limiter out of the measurement */
    p.tweeter_ceiling = 1.0f;

    hk_eq_settings_t start = hk_eq_defaults();
    start.band[0].hz = 100.0f;
    start.band[0].gain_db = 12.0f;
    start.band[0].q = 0.71f;

    /* Same settings plus one band the low shelf knows nothing about. */
    hk_eq_settings_t edited = start;
    edited.band[2].hz = 10000.0f;
    edited.band[2].gain_db = 3.0f;

    hk_dsp_t settled;
    hk_dsp_t switched;
    HK_CHECK(build(&settled, &p, &edited));
    HK_CHECK(build(&switched, &p, &start));

    int16_t a[BLOCK * 2u];
    int16_t b[BLOCK * 2u];
    double  pa = 0.0;
    double  pb = 0.0;
    for (size_t k = 0; k < 200u; k++) {
        tone_block(a, &pa);
        HK_CHECK(hk_dsp_process(&settled, a, BLOCK));
        tone_block(b, &pb);
        HK_CHECK(hk_dsp_process(&switched, b, BLOCK));
    }

    const float shelf_z1 = switched.eq.stage[0].state.z1;
    HK_CHECK(shelf_z1 != 0.0f);

    HK_CHECK(hk_dsp_set_eq(&switched, &edited));

    /* The untouched band kept its memory, bit for bit. That is the mechanism;
     * the deviation below is the consequence. */
    HK_CHECK(switched.eq.stage[0].state.z1 == shelf_z1);
    HK_CHECK(switched.eq.stage[0].active);

    int worst = 0;
    for (size_t k = 0; k < 8u; k++) {
        tone_block(a, &pa);
        HK_CHECK(hk_dsp_process(&settled, a, BLOCK));
        tone_block(b, &pb);
        HK_CHECK(hk_dsp_process(&switched, b, BLOCK));
        for (size_t i = 0; i < BLOCK * 2u; i++) {
            const int d = abs((int)b[i] - (int)a[i]);
            if (d > worst) {
                worst = d;
            }
        }
    }

    /* 980 LSB with the carry-over, and what remains is the 10 kHz band starting
     * from rest, which is a real change to the response rather than lost
     * history. 9029 without it. The bound is set between the two, close enough
     * to the achieved figure to fail if the carry-over is removed or narrowed. */
    HK_CHECK(worst < 2000);

    /* A band that genuinely changed is allowed to restart from rest: the
     * carry-over is keyed on the coefficients coming out identical, not on the
     * band index, so moving a corner does not smuggle stale memory into a
     * different filter. */
    hk_eq_settings_t moved = edited;
    moved.band[0].hz = 200.0f;
    HK_CHECK(hk_dsp_set_eq(&switched, &moved));
    HK_CHECK(switched.eq.stage[0].state.z1 == 0.0f);

    /* And the protective half is untouched by any of it. */
    HK_CHECK(hk_dsp_ready(&switched));
}

/**
 * The filters must not sit in the subnormal range while the speaker is idle.
 *
 * The backend calls hk_dsp_process() on every silence frame on purpose, so the
 * filters ring out across a gap instead of freezing mid-decay. The subsonic
 * section's poles are at radius 0.993, so its memory reaches FLT_MIN about
 * 0.2 s after the music stops and then stops decaying cleanly -- subnormals
 * carry fewer mantissa bits and the recurrence becomes a limit cycle. Measured
 * before the flush: subnormal state in 373 of 400 silence blocks, i.e. for as
 * long as the speaker is idle.
 */
static void dsp_state_never_sits_subnormal(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    int16_t buf[BLOCK * 2u];
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = 30000;
    }
    for (size_t b = 0; b < 8u; b++) {
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
    }

    /* 400 blocks is 3.2 s of silence -- well past the 0.2 s where the state
     * used to cross into the subnormal range and stay there. */
    for (size_t b = 0; b < 400u; b++) {
        memset(buf, 0, sizeof(buf));
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));

        const float z[13] = {
            dsp.hpf_state.section[0].z1,  dsp.hpf_state.section[0].z2,
            dsp.hpf_state.section[1].z1,  dsp.hpf_state.section[1].z2,
            dsp.low_state.section[0].z1,  dsp.low_state.section[1].z1,
            dsp.high_state.section[0].z1, dsp.high_state.section[1].z1,
            dsp.eq.stage[0].state.z1,     dsp.eq.stage[1].state.z1,
            dsp.eq.stage[2].state.z1,     dsp.low_state.section[0].z2,
            dsp.supply_limit.mean_sq,
        };
        for (size_t k = 0; k < 13u; k++) {
            const float magnitude = fabsf(z[k]);
            HK_CHECK(magnitude == 0.0f || magnitude >= FLT_MIN);
        }
    }

    /* Flushing must not have cost the path anything: it still plays. */
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = 20000;
    }
    HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
    HK_CHECK(hk_dsp_ready(&dsp));
}

/** Nothing in the path may turn a broken sample into a broken output. */
static void dsp_survives_extremes(void)
{
    const hk_profile_t p = fixture_profile();
    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    int16_t buf[BLOCK * 2u];
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        buf[i] = (int16_t)((i % 2u) ? -32768 : 32767);
    }
    HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));

    /* Zero frames is not an error, and must not walk off the buffer. */
    HK_CHECK(hk_dsp_process(&dsp, buf, 0u));
    HK_CHECK(!hk_dsp_process(&dsp, NULL, BLOCK));
    HK_CHECK(!hk_dsp_process(NULL, buf, BLOCK));
    hk_dsp_reset(NULL);
    HK_CHECK(!hk_dsp_set_eq(NULL, NULL));
    HK_CHECK(!hk_dsp_ready(NULL));
    HK_CHECK_EQ_STR(hk_dsp_refusal_name(HK_DSP_OK), "ok");
}

/**
 * Run @p blocks blocks of a fixed two-tone programme through @p dsp and
 * collect the output, so two instances can be compared sample by sample.
 *
 * Two tones, one either side of the crossover, so both branches carry
 * something; amplitude well under full scale so no peak limiter engages and
 * the comparison is about the stages under test rather than about gain
 * reduction that happens to fall differently.
 */
static void run_two_tone(hk_dsp_t *dsp, int16_t *out, size_t blocks)
{
    double phase_a = 0.0;
    double phase_b = 0.0;
    int16_t buf[BLOCK * 2u];
    for (size_t b = 0; b < blocks; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            const double v = 0.25 * sin(phase_a) + 0.25 * sin(phase_b);
            phase_a += 2.0 * M_PI * 300.0 / (double)FS_HZ;
            phase_b += 2.0 * M_PI * 9000.0 / (double)FS_HZ;
            const int16_t s = (int16_t)(v * 32767.0);
            buf[2u * i] = s;
            buf[2u * i + 1u] = s;
        }
        HK_CHECK(hk_dsp_process(dsp, buf, BLOCK));
        memcpy(out + b * BLOCK * 2u, buf, sizeof(buf));
    }
}

#define RUN_BLOCKS 24u
#define RUN_FRAMES (RUN_BLOCKS * BLOCK)

/**
 * The alignment delay is sample-exact and touches one branch only.
 *
 * With tweeter_delay_samples = N, the tweeter output is the N = 0 run shifted
 * by exactly N samples -- not approximately, since a ring of floats holds the
 * pre-rounded value -- and the woofer output is bit-identical. Then the same
 * with the woofer delayed, since the two rings are separate code paths.
 */
static void dsp_delay_is_sample_exact(void)
{
    static int16_t reference[RUN_FRAMES * 2u];
    static int16_t delayed[RUN_FRAMES * 2u];
    const uint32_t n = 17u;

    hk_profile_t p = fixture_profile();
    hk_dsp_t base;
    HK_CHECK(build(&base, &p, NULL));
    run_two_tone(&base, reference, RUN_BLOCKS);

    p.tweeter_delay_samples = n;
    hk_dsp_t shifted;
    HK_CHECK(build(&shifted, &p, NULL));
    HK_CHECK(shifted.chain.tweeter_delay_samples == n);
    run_two_tone(&shifted, delayed, RUN_BLOCKS);

    size_t woofer_mismatch = 0u;
    size_t tweeter_mismatch = 0u;
    for (size_t i = 0; i < RUN_FRAMES; i++) {
        if (delayed[2u * i] != reference[2u * i]) {
            woofer_mismatch++;
        }
        const int16_t expect = (i < n) ? 0 : reference[2u * (i - n) + 1u];
        if (delayed[2u * i + 1u] != expect) {
            tweeter_mismatch++;
        }
    }
    HK_CHECK_EQ_INT(woofer_mismatch, 0);
    HK_CHECK_EQ_INT(tweeter_mismatch, 0);

    /* The other ring, the other way round. */
    p = fixture_profile();
    p.woofer_delay_samples = n;
    HK_CHECK(build(&shifted, &p, NULL));
    run_two_tone(&shifted, delayed, RUN_BLOCKS);
    woofer_mismatch = 0u;
    tweeter_mismatch = 0u;
    for (size_t i = 0; i < RUN_FRAMES; i++) {
        const int16_t expect = (i < n) ? 0 : reference[2u * (i - n)];
        if (delayed[2u * i] != expect) {
            woofer_mismatch++;
        }
        if (delayed[2u * i + 1u] != reference[2u * i + 1u]) {
            tweeter_mismatch++;
        }
    }
    HK_CHECK_EQ_INT(woofer_mismatch, 0);
    HK_CHECK_EQ_INT(tweeter_mismatch, 0);

    /* The bound itself works, and it is where the header says it is. */
    p = fixture_profile();
    p.tweeter_delay_samples = HK_PROFILE_DELAY_MAX_SAMPLES;
    HK_CHECK(build(&shifted, &p, NULL));
    run_two_tone(&shifted, delayed, RUN_BLOCKS);
    tweeter_mismatch = 0u;
    for (size_t i = 0; i < RUN_FRAMES; i++) {
        const int16_t expect = (i < HK_PROFILE_DELAY_MAX_SAMPLES)
                                   ? 0
                                   : reference[2u * (i - HK_PROFILE_DELAY_MAX_SAMPLES) + 1u];
        if (delayed[2u * i + 1u] != expect) {
            tweeter_mismatch++;
        }
    }
    HK_CHECK_EQ_INT(tweeter_mismatch, 0);
    HK_CHECK_EQ_INT(HK_PROFILE_DELAY_MAX_SAMPLES, 64);

    /* Reset clears the ring: an impulse after a reset arrives at N, not at N
     * plus whatever the ring still held from the run before. */
    HK_CHECK(shifted.tweeter_delay_index != 0u || shifted.tweeter_delay[1] != 0.0f);
    hk_dsp_reset(&shifted);
    for (size_t i = 0; i < HK_PROFILE_DELAY_MAX_SAMPLES; i++) {
        HK_CHECK(shifted.tweeter_delay[i] == 0.0f);
    }
    HK_CHECK(shifted.tweeter_delay_index == 0u);
    int16_t buf[BLOCK * 2u];
    memset(buf, 0, sizeof(buf));
    HK_CHECK(hk_dsp_process(&shifted, buf, BLOCK));
    for (size_t i = 0; i < BLOCK * 2u; i++) {
        HK_CHECK_EQ_INT(buf[i], 0);
    }
}

/** Polarity 1 is the exact negation of polarity 0, on the tweeter only. */
static void dsp_polarity_negates_the_tweeter(void)
{
    static int16_t reference[RUN_FRAMES * 2u];
    static int16_t inverted[RUN_FRAMES * 2u];

    hk_profile_t p = fixture_profile();
    hk_dsp_t base;
    HK_CHECK(build(&base, &p, NULL));
    run_two_tone(&base, reference, RUN_BLOCKS);

    p.tweeter_polarity = 1u;
    hk_dsp_t flipped;
    HK_CHECK(build(&flipped, &p, NULL));
    HK_CHECK(flipped.chain.tweeter_sign == -1.0f);
    run_two_tone(&flipped, inverted, RUN_BLOCKS);

    size_t woofer_mismatch = 0u;
    size_t tweeter_mismatch = 0u;
    size_t nonzero = 0u;
    for (size_t i = 0; i < RUN_FRAMES; i++) {
        if (inverted[2u * i] != reference[2u * i]) {
            woofer_mismatch++;
        }
        /* to_i16 rounds symmetrically about zero, so -x maps to -(x) exactly
         * short of full scale, and the programme sits well short of it. */
        if (inverted[2u * i + 1u] != (int16_t)-reference[2u * i + 1u]) {
            tweeter_mismatch++;
        }
        if (reference[2u * i + 1u] != 0) {
            nonzero++;
        }
    }
    HK_CHECK_EQ_INT(woofer_mismatch, 0);
    HK_CHECK_EQ_INT(tweeter_mismatch, 0);
    HK_CHECK(nonzero > RUN_FRAMES / 2u); /* the tweeter branch carried signal */

    /* And the sum measurement sees it: the coherent sum at the corner is a
     * notch, which is exactly the wiring mistake hk_biquad.h warns about --
     * now a stored choice the bench can make, rather than a mistake. */
    hk_profile_t equal = fixture_profile_equal_gains();
    equal.tweeter_polarity = 1u;
    HK_CHECK(build(&flipped, &equal, NULL));
    float w, t, sum;
    measure(&flipped, PROVISIONAL_CROSSOVER_HZ, 0.5f, &w, &t, &sum);
    HK_CHECK(sum < -20.0f);
}

/**
 * The supply stage engages on a loud two-tone and not on a quiet one, and
 * when it engages the output's mean square lands on the budget while the
 * woofer/tweeter ratio is preserved.
 */
static void dsp_supply_stage_holds_the_budget(void)
{
    hk_profile_t p = fixture_profile_equal_gains();
    p.supply_budget_sq = 0.02f;
    p.supply_window_ms = 20u;

    hk_dsp_t dsp;
    HK_CHECK(build(&dsp, &p, NULL));

    /* Quiet: two tones at 0.05 each, mean square 2 x 0.05^2/2 = 0.0025,
     * an eighth of the budget. Unity on every block, exactly. The branch
     * ratio the crossover alone produces is measured here, for the loud run
     * to be compared against. */
    int16_t buf[BLOCK * 2u];
    double phase_a = 0.0;
    double phase_b = 0.0;
    double quiet_w_sq = 0.0;
    double quiet_t_sq = 0.0;
    for (size_t b = 0; b < 60u; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            const double v = 0.05 * sin(phase_a) + 0.05 * sin(phase_b);
            phase_a += 2.0 * M_PI * 300.0 / (double)FS_HZ;
            phase_b += 2.0 * M_PI * 9000.0 / (double)FS_HZ;
            buf[2u * i] = (int16_t)(v * 32767.0);
            buf[2u * i + 1u] = buf[2u * i];
        }
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
        HK_CHECK(dsp.supply_limit.gain == 1.0f);
        if (b >= 50u) {
            for (size_t i = 0; i < BLOCK; i++) {
                const double w = (double)buf[2u * i] / 32768.0;
                const double t = (double)buf[2u * i + 1u] / 32768.0;
                quiet_w_sq += w * w;
                quiet_t_sq += t * t;
            }
        }
    }

    /* Loud: 0.4 each, mean square 2 x 0.4^2/2 = 0.16 at the input, about
     * eight times the budget once the crossover has taken its fraction of a
     * dB off the upper tone. The stage engages and the OUTPUT mean square,
     * measured over the last blocks once the detector has settled, is the
     * budget within 5%. */
    hk_dsp_reset(&dsp);
    double out_sq = 0.0;
    double w_sq = 0.0;
    double t_sq = 0.0;
    size_t n = 0u;
    for (size_t b = 0; b < 60u; b++) {
        for (size_t i = 0; i < BLOCK; i++) {
            const double v = 0.4 * sin(phase_a) + 0.4 * sin(phase_b);
            phase_a += 2.0 * M_PI * 300.0 / (double)FS_HZ;
            phase_b += 2.0 * M_PI * 9000.0 / (double)FS_HZ;
            buf[2u * i] = (int16_t)(v * 32767.0);
            buf[2u * i + 1u] = buf[2u * i];
        }
        HK_CHECK(hk_dsp_process(&dsp, buf, BLOCK));
        if (b >= 50u) {
            HK_CHECK(dsp.supply_limit.gain < 0.5f);
            for (size_t i = 0; i < BLOCK; i++) {
                const double w = (double)buf[2u * i] / 32768.0;
                const double t = (double)buf[2u * i + 1u] / 32768.0;
                out_sq += w * w + t * t;
                w_sq += w * w;
                t_sq += t * t;
                n++;
            }
        }
    }
    out_sq /= (double)n;
    HK_CHECK(fabs(out_sq - 0.02) < 0.02 * 0.05);

    /* A COMMON gain leaves the two branches in the ratio the crossover gave
     * them: the loud run's woofer/tweeter ratio equals the quiet run's. This
     * is what a per-branch stage would get wrong. */
    HK_CHECK(fabs((w_sq / t_sq) / (quiet_w_sq / quiet_t_sq) - 1.0) < 0.02);

    /* The gain it settled at is about the one the law predicts, sqrt(1/8);
     * a little above it because the crossover trimmed the upper tone. */
    HK_CHECK(dsp.supply_limit.gain > 0.33f && dsp.supply_limit.gain < 0.40f);
}

void test_dsp(void)
{
    eq_defaults_are_flat();
    eq_settings_table_is_spliceable();
    eq_settings_decode();
    eq_design_refuses_the_impossible();
    eq_rejects_one_band_not_the_chain();

    dsp_refuses_without_a_profile();
    dsp_refuses_a_chain_from_another_rate();
    dsp_refuses_a_corrupt_chain();
    dsp_limiter_cannot_be_bypassed();
    dsp_ceiling_holds_under_a_boost();
    dsp_eq_cannot_touch_the_chain();
    dsp_maps_channels_to_drivers();
    dsp_response_matches_the_profile();
    dsp_applies_the_branch_gains();
    dsp_eq_does_what_it_says();
    dsp_reset_clears_state();
    dsp_set_eq_does_not_click_the_other_bands();
    dsp_state_never_sits_subnormal();
    dsp_survives_extremes();
    dsp_delay_is_sample_exact();
    dsp_polarity_negates_the_tweeter();
    dsp_supply_stage_holds_the_budget();
}
