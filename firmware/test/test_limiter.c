#include "hk_test.h"
#include "hk_limiter.h"

#include <math.h>
#include <string.h>

/*
 * One property matters more than all the others put together: the output never
 * exceeds the ceiling. Everything else here is about not making that guarantee
 * expensive in ways a listener would notice.
 *
 * The ceiling values below are placeholders. The real one comes from G2 and
 * measured driver behaviour; nothing in this file should be read as a tuning
 * decision.
 */

static hk_limiter_config_t config(void)
{
    hk_limiter_config_t c = {
        .ceiling = 0.5f,
        .release_ms = 100,
        .hold_ms = 10,
        .sample_rate = 44100,
    };
    return c;
}

void test_limiter(void)
{
    hk_limiter_config_t cfg = config();
    hk_limiter_t lim;

    /* ===== a configuration that cannot work is refused ===== */
    HK_CHECK(hk_limiter_config_valid(&cfg));
    HK_CHECK(!hk_limiter_config_valid(NULL));

    {
        hk_limiter_config_t bad;

        bad = config(); bad.ceiling = 0.0f;      /* silences the output */
        HK_CHECK(!hk_limiter_config_valid(&bad));
        bad = config(); bad.ceiling = -0.5f;
        HK_CHECK(!hk_limiter_config_valid(&bad));
        bad = config(); bad.ceiling = 1.5f;      /* never engages */
        HK_CHECK(!hk_limiter_config_valid(&bad));
        bad = config(); bad.ceiling = NAN;       /* an unset float */
        HK_CHECK(!hk_limiter_config_valid(&bad));
        bad = config(); bad.sample_rate = 0;
        HK_CHECK(!hk_limiter_config_valid(&bad));
        bad = config(); bad.release_ms = 0;      /* a switch, not a limiter */
        HK_CHECK(!hk_limiter_config_valid(&bad));

        /* and a limiter built from one refuses to run rather than passing
         * audio through unprotected */
        bad = config(); bad.ceiling = 0.0f;
        HK_CHECK(!hk_limiter_init(&lim, &bad));
        float sample = 0.9f;
        HK_CHECK(!hk_limiter_process(&lim, &sample, 1));
        HK_CHECK(sample == 0.9f);            /* untouched: the caller must not play it */
    }

    /* ===== quiet audio is left alone ===== */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        float quiet[] = {0.0f, 0.1f, -0.2f, 0.4f, -0.49f};
        float before[5];
        memcpy(before, quiet, sizeof(quiet));
        HK_CHECK(hk_limiter_process(&lim, quiet, 5));
        for (int i = 0; i < 5; i++) {
            HK_CHECK(quiet[i] == before[i]);
        }
        HK_CHECK(lim.gain == 1.0f);
    }

    /* ===== the guarantee, over a wide sweep =====
     * No attack time means no overshoot: the gain that brings a sample under
     * the ceiling is computed from that sample and applied to it. This is the
     * property that makes it protection rather than decoration. */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    for (int i = -2000; i <= 2000; i++) {
        const float sample = (float)i / 100.0f;   /* -20.0 .. +20.0 full scale */
        float value = sample;
        HK_CHECK(hk_limiter_process(&lim, &value, 1));
        HK_CHECK(fabsf(value) <= cfg.ceiling + 1e-6f);
        /* and the limiter never amplifies */
        HK_CHECK(lim.gain <= 1.0f);
        HK_CHECK(lim.gain > 0.0f);
    }

    /* ===== a sample over the ceiling lands exactly on it ===== */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        float loud = 1.0f;
        HK_CHECK(hk_limiter_process(&lim, &loud, 1));
        HK_CHECK(fabsf(loud - cfg.ceiling) < 1e-6f);

        /* including on the negative half */
        HK_CHECK(hk_limiter_init(&lim, &cfg));
        loud = -1.0f;
        HK_CHECK(hk_limiter_process(&lim, &loud, 1));
        HK_CHECK(fabsf(loud + cfg.ceiling) < 1e-6f);
    }

    /* ===== hold keeps the gain down between two transients =====
     * Without it, the gain recovers in the dip and the second transient
     * arrives at full gain — which is the one it was supposed to catch. */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        float peak = 1.0f;
        (void)hk_limiter_step(&lim, peak);
        const float after_peak = lim.gain;
        HK_CHECK(after_peak < 1.0f);

        /* silence for less than the hold time */
        for (uint32_t s = 0; s < lim.hold_samples / 2u; s++) {
            (void)hk_limiter_step(&lim, 0.0f);
        }
        HK_CHECK(lim.gain == after_peak);    /* not a decibel of recovery */

        /* past the hold, it starts coming back */
        for (uint32_t s = 0; s < lim.hold_samples; s++) {
            (void)hk_limiter_step(&lim, 0.0f);
        }
        HK_CHECK(lim.gain > after_peak);
    }

    /* ===== release is gradual, and finishes ===== */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        (void)hk_limiter_step(&lim, 1.0f);
        const float reduced = lim.gain;

        /* one release time constant should recover roughly 63% of the way */
        const uint32_t one_tau = (cfg.release_ms * cfg.sample_rate) / 1000u;
        for (uint32_t s = 0; s < lim.hold_samples + one_tau; s++) {
            (void)hk_limiter_step(&lim, 0.0f);
        }
        const float expected = 1.0f - (1.0f - reduced) * expf(-1.0f);
        HK_CHECK(fabsf(lim.gain - expected) < 0.02f);

        /* and given long enough it returns to unity without passing it */
        for (uint32_t s = 0; s < 20u * one_tau; s++) {
            (void)hk_limiter_step(&lim, 0.0f);
            HK_CHECK(lim.gain <= 1.0f);
        }
        HK_CHECK(lim.gain > 0.999f);
    }

    /* ===== a sustained loud signal stays limited ===== */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    for (int i = 0; i < 10000; i++) {
        float value = 0.9f;
        (void)hk_limiter_process(&lim, &value, 1);
        HK_CHECK(value <= cfg.ceiling + 1e-6f);
    }

    /* ===== a broken sample must not poison the gain ===== */
    /* A NaN multiplied into the gain would silence every sample afterwards,
     * turning one bad value into a dead speaker. */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        (void)hk_limiter_step(&lim, 1.0f);

        /* Past the hold, so recovery is actually running. Feeding NaN while
         * the limiter is holding would prove nothing: the gain is frozen
         * either way. */
        for (uint32_t s = 0; s <= lim.hold_samples; s++) {
            (void)hk_limiter_step(&lim, 0.0f);
        }
        const float before = lim.gain;
        HK_CHECK(before < 1.0f);

        const float g = hk_limiter_step(&lim, NAN);
        HK_CHECK(!isnan(g));
        /* A broken sample is not silence. Letting it advance the release would
         * mean a stream of NaNs quietly returns the limiter to full gain. */
        HK_CHECK(lim.gain == before);
        /* and the limiter still works afterwards */
        float value = 1.0f;
        HK_CHECK(hk_limiter_process(&lim, &value, 1));
        HK_CHECK(fabsf(value) <= cfg.ceiling + 1e-6f);
    }

    /* ===== an infinite sample must not silence what follows =====
     * Found by audit. The guard used to test isnan alone, so an infinity got
     * through: fabsf(INFINITY) exceeds any ceiling, the reduction ran, and
     * gain became ceiling/INFINITY = 0 with the hold pinning it there. One bad
     * sample muted the output for the whole hold time — about 10 ms at these
     * settings — which is the opposite of what a protection stage is for. */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        float buf[4] = {0.1f, INFINITY, 0.1f, 0.1f};
        HK_CHECK(hk_limiter_process(&lim, buf, 4));
        HK_CHECK(buf[0] == 0.1f);
        /* the broken sample stays broken; this stage does not repair it */
        HK_CHECK(!isfinite(buf[1]));
        /* but the ones after it are untouched */
        HK_CHECK(buf[2] == 0.1f);
        HK_CHECK(buf[3] == 0.1f);
        HK_CHECK(lim.gain == 1.0f);
        HK_CHECK(lim.held == 0u);
    }
    /* the negative infinity behaves the same way */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    {
        float buf[2] = {-INFINITY, 0.2f};
        HK_CHECK(hk_limiter_process(&lim, buf, 2));
        HK_CHECK(buf[1] == 0.2f);
        HK_CHECK(lim.gain == 1.0f);
    }
    /* and a stream of them does not accumulate anything */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    for (int i = 0; i < 500; i++) {
        float v = (i % 2) ? INFINITY : NAN;
        (void)hk_limiter_process(&lim, &v, 1);
    }
    HK_CHECK(lim.gain == 1.0f);
    {
        float after = 1.0f;
        HK_CHECK(hk_limiter_process(&lim, &after, 1));
        HK_CHECK(fabsf(after - cfg.ceiling) < 1e-6f);
    }

    /* ===== degenerate calls ===== */
    HK_CHECK(!hk_limiter_process(NULL, NULL, 0));
    HK_CHECK(!hk_limiter_init(NULL, &cfg));
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    HK_CHECK(hk_limiter_process(&lim, NULL, 0));   /* nothing to do is not a failure */
    HK_CHECK(!hk_limiter_process(&lim, NULL, 4));  /* but a null block of samples is */
    HK_CHECK(hk_limiter_step(NULL, 1.0f) == 1.0f);

    /* ===== the invariant that replaced the per-sample clamp ===== */
    HK_CHECK(hk_limiter_init(&lim, &cfg));
    HK_CHECK(lim.release_coeff >= 0.0f && lim.release_coeff < 1.0f);
    {
        /* A very short release still yields a usable coefficient rather than
         * one that would let the recurrence overshoot unity. */
        hk_limiter_config_t fast = config();
        fast.release_ms = 1;
        HK_CHECK(hk_limiter_init(&lim, &fast));
        HK_CHECK(lim.release_coeff >= 0.0f && lim.release_coeff < 1.0f);

        /* And a long one that still works. */
        hk_limiter_config_t slow = config();
        slow.release_ms = 60000;
        HK_CHECK(hk_limiter_init(&lim, &slow));
        HK_CHECK(lim.release_coeff >= 0.0f && lim.release_coeff < 1.0f);

        /* A release so long that expf(-1/samples) rounds to exactly 1.0f in
         * single precision. Measured: at 44.1 kHz this happens somewhere
         * between 400000 and 1000000 ms. The recurrence would then be
         * gain = 1 - (1 - gain) * 1.0, which is gain — the limiter would
         * reduce once on the first peak and never recover, silently, for as
         * long as the speaker stayed on. The invariant refuses it instead. */
        hk_limiter_config_t frozen = config();
        frozen.release_ms = 1000000;
        HK_CHECK(!hk_limiter_init(&lim, &frozen));
        HK_CHECK(!lim.ready);
        /* and a limiter that refused to initialise will not pass audio */
        float sample = 0.9f;
        HK_CHECK(!hk_limiter_process(&lim, &sample, 1));
        HK_CHECK(sample == 0.9f);
    }
}
