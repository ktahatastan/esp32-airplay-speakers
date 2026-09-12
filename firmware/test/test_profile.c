#include "hk_test.h"
#include "hk_profile.h"

#include <math.h>
#include <string.h>

/*
 * NOT DRIVER VALUES. Every number below is invented for the arithmetic and
 * must never be copied into a device: the Nova woofer and tweeter have not
 * been measured (G0). What is under test is the FORM -- which profiles are
 * refused, and how a ceiling moves with the supply voltage -- and none of that
 * depends on the numbers being the real ones.
 */
static hk_profile_t sample(void)
{
    hk_profile_t p;
    memset(&p, 0, sizeof(p));
    p.schema = HK_PROFILE_SCHEMA;
    p.measured_yyyymmdd = 20260908u;
    (void)snprintf(p.source, sizeof(p.source), "synthetic-not-a-measurement");
    p.woofer_dcr_ohm = 6.0f;
    p.tweeter_dcr_ohm = 5.0f;
    p.woofer_hpf_hz = 45.0f;
    p.crossover_hz = 2200.0f;
    p.woofer_gain = 0.9f;
    p.tweeter_gain = 0.7f;
    p.reference_supply_mv = 19000.0f;
    p.woofer_ceiling = 0.8f;
    p.tweeter_ceiling = 0.5f;
    p.release_ms = 120u;
    p.hold_ms = 30u;
    return p;
}

static bool close_to(float a, float b)
{
    return fabsf(a - b) < 1.0e-5f;
}

void test_profile(void)
{
    /* ===== a well-formed profile is accepted ===== */
    {
        const hk_profile_t p = sample();
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
    }

    /* ===== every refusal names itself ===== */
    HK_CHECK_EQ_INT(hk_profile_valid(NULL), HK_PROFILE_BAD_SCHEMA);
    {
        hk_profile_t p = sample();
        p.schema = HK_PROFILE_SCHEMA + 1u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_SCHEMA);
    }
    {   /* A profile that cannot name its measurement is not traceable to one. */
        hk_profile_t p = sample();
        p.source[0] = '\0';
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_NO_SOURCE);

        p = sample();
        p.measured_yyyymmdd = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_NO_SOURCE);

        /* An unterminated source would run every later reader off the struct. */
        p = sample();
        memset(p.source, 'x', sizeof(p.source));
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_NO_SOURCE);
    }
    {
        hk_profile_t p = sample();
        p.woofer_dcr_ohm = 0.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_MEASUREMENT);
        p = sample();
        p.tweeter_dcr_ohm = -1.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_MEASUREMENT);
    }
    {   /* Swapped corners: the woofer branch would have nothing left in it. */
        hk_profile_t p = sample();
        p.woofer_hpf_hz = 3000.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_FREQUENCY);

        p = sample();
        p.woofer_hpf_hz = p.crossover_hz;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_FREQUENCY);

        p = sample();
        p.crossover_hz = 0.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_FREQUENCY);
    }
    {
        hk_profile_t p = sample();
        p.tweeter_gain = 1.5f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_GAIN);
        p = sample();
        p.woofer_gain = 0.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_GAIN);
    }
    {
        hk_profile_t p = sample();
        p.tweeter_ceiling = 0.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_CEILING);
    }
    {   /* A ceiling measured at a voltage this amplifier cannot be fed belongs
         * to some other speaker. */
        hk_profile_t p = sample();
        p.reference_supply_mv = 5000.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_REFERENCE);
        p = sample();
        p.reference_supply_mv = 30000.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_REFERENCE);
    }
    {
        hk_profile_t p = sample();
        p.release_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_TIMING);
    }
    {   /* NaN passes every naive comparison; it must not pass this one. */
        hk_profile_t p = sample();
        p.crossover_hz = NAN;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_FREQUENCY);
        p = sample();
        p.woofer_gain = NAN;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_GAIN);
    }

    /* ===== stored bytes ===== */
    {
        const hk_profile_t written = sample();
        hk_profile_t read;

        HK_CHECK_EQ_INT(hk_profile_from_blob(&written, sizeof(written), &read),
                        HK_PROFILE_OK);
        HK_CHECK(close_to(read.crossover_hz, written.crossover_hz));
        HK_CHECK(strcmp(read.source, written.source) == 0);

        /* A blob of the wrong length is not an older profile, it is not one. */
        HK_CHECK_EQ_INT(hk_profile_from_blob(&written, sizeof(written) - 1u, &read),
                        HK_PROFILE_BAD_SCHEMA);
        HK_CHECK_EQ_INT(hk_profile_from_blob(NULL, sizeof(written), &read),
                        HK_PROFILE_BAD_SCHEMA);

        /* A refusal must not leave a usable profile behind. */
        hk_profile_t broken = sample();
        broken.release_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_from_blob(&broken, sizeof(broken), &read),
                        HK_PROFILE_BAD_TIMING);
        HK_CHECK(read.schema == 0u && read.crossover_hz == 0.0f);
    }

    /* ===== the ceiling follows the supply, the other way round ===== */
    {
        /* At the voltage it was measured at, it is itself. */
        HK_CHECK(close_to(hk_profile_ceiling_at(0.5f, 19000.0f, 19000.0f), 0.5f));

        /* A higher supply puts more volts on the driver per digital unit, so
         * the digital ceiling has to come DOWN. This is the direction that
         * keeps a tweeter alive when someone plugs in a 24 V adapter, so it is
         * asserted rather than assumed. */
        HK_CHECK(hk_profile_ceiling_at(0.5f, 19000.0f, 24000.0f) < 0.5f);
        HK_CHECK(close_to(hk_profile_ceiling_at(0.5f, 19000.0f, 24000.0f),
                          0.5f * 19000.0f / 24000.0f));

        /* A lower supply allows more digital level for the same volts. */
        HK_CHECK(hk_profile_ceiling_at(0.5f, 19000.0f, 12000.0f) > 0.5f);
        HK_CHECK(close_to(hk_profile_ceiling_at(0.5f, 19000.0f, 12000.0f),
                          0.5f * 19000.0f / 12000.0f));

        /* Full scale is the end of the signal; there is nothing above it. */
        HK_CHECK(close_to(hk_profile_ceiling_at(0.9f, 19000.0f, 12000.0f), 1.0f));

        /* Nonsense in, zero out -- and zero means do not play, not silence. */
        HK_CHECK(hk_profile_ceiling_at(0.5f, 19000.0f, 0.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.5f, 19000.0f, -1.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.0f, 19000.0f, 19000.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.5f, NAN, 19000.0f) == 0.0f);
    }

    /* ===== building a chain ===== */
    {
        const hk_profile_t p = sample();
        hk_profile_chain_t chain;

        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 19000.0f, &chain), HK_PROFILE_OK);
        HK_CHECK(hk_biquad_stable(&chain.woofer_hpf));
        HK_CHECK(hk_biquad_stable(&chain.woofer_low.section[0]));
        HK_CHECK(hk_biquad_stable(&chain.woofer_low.section[1]));
        HK_CHECK(hk_biquad_stable(&chain.tweeter_high.section[0]));
        HK_CHECK(hk_biquad_stable(&chain.tweeter_high.section[1]));
        HK_CHECK(chain.woofer_limit.sample_rate == 44100u);
        HK_CHECK(chain.tweeter_limit.release_ms == p.release_ms);
        HK_CHECK(hk_limiter_config_valid(&chain.woofer_limit));
        HK_CHECK(hk_limiter_config_valid(&chain.tweeter_limit));
        HK_CHECK(close_to(chain.woofer_gain, p.woofer_gain));

        /* The built ceiling is the scaled one, not the stored one. */
        HK_CHECK(close_to(chain.tweeter_limit.ceiling,
                          hk_profile_ceiling_at(p.tweeter_ceiling, p.reference_supply_mv,
                                                19000.0f)));

        /* Same profile, lower supply: the ceilings move and nothing else does. */
        hk_profile_chain_t low;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 12000.0f, &low), HK_PROFILE_OK);
        HK_CHECK(low.tweeter_limit.ceiling > chain.tweeter_limit.ceiling);
        HK_CHECK(close_to(low.woofer_low.section[0].b0, chain.woofer_low.section[0].b0));
    }

    /* ===== a profile that cannot be built at this rate is refused, not moved ===== */
    {
        hk_profile_t p = sample();
        /* Above Nyquist the design has no meaning; silently pulling it down
         * would produce a crossover nobody chose. */
        p.crossover_hz = 30000.0f;
        hk_profile_chain_t chain;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 19000.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);

        p = sample();
        HK_CHECK_EQ_INT(hk_profile_build(&p, 0.0f, 19000.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 0.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);
        HK_CHECK_EQ_INT(hk_profile_build(NULL, 44100.0f, 19000.0f, &chain),
                        HK_PROFILE_BAD_SCHEMA);

        /* An invalid profile fails with its own reason, not a generic one. */
        p = sample();
        p.tweeter_gain = 2.0f;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 19000.0f, &chain),
                        HK_PROFILE_BAD_GAIN);
    }

    /* ===== every verdict has a name, so a log line can carry it ===== */
    for (int v = HK_PROFILE_OK; v <= HK_PROFILE_UNBUILDABLE; v++) {
        const char *name = hk_profile_verdict_name((hk_profile_verdict_t)v);
        HK_CHECK(name != NULL && strcmp(name, "unknown") != 0);
    }
}
