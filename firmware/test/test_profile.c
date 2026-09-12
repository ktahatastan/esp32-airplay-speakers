#include "hk_test.h"
#include "hk_profile.h"

#include <math.h>
#include <string.h>

/*
 * NOT DRIVER VALUES. Every number below is invented for the arithmetic and
 * must never be copied into a device: beyond their DC resistances the Nova
 * woofer and tweeter have not been measured (G0). What is under test is the
 * FORM -- which profiles are refused, and how a ceiling moves with the supply
 * voltage -- and none of that depends on the numbers being the real ones.
 *
 * The two supplies that appear are the product's: 24000 mV is the adapter
 * (ADR-0020) and 12000 mV is the bench reference a profile may have been
 * listened to at (HK_BENCH_REFERENCE_SUPPLY_MV).
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
    p.reference_supply_mv = 24000.0f;
    p.woofer_ceiling = 0.8f;
    p.tweeter_ceiling = 0.5f;
    p.woofer_release_ms = 120u;
    p.woofer_hold_ms = 30u;
    /* Schema 2. Different per branch on purpose, so a build that copied the
     * woofer's timing into both limiters would be caught. */
    p.tweeter_release_ms = 80u;
    p.tweeter_hold_ms = 15u;
    p.woofer_delay_samples = 0u;
    p.tweeter_delay_samples = 0u;
    p.tweeter_polarity = 0u;
    p.supply_budget_sq = 0.3f;
    p.supply_window_ms = 50u;
    p.amp_gain_db = 0u; /* C3 not done: accepted, and carried as "unread" */
    return p;
}

static bool close_to(float a, float b)
{
    return fabsf(a - b) < 1.0e-5f;
}

void test_profile(void)
{
    /* ===== the wire format is what the header says it is =====
     * A literal, so that a reorder, a field of the wrong width or a padding
     * byte the compiler inserted is caught here rather than on a device that
     * refuses a blob written by an older build. 40 bytes of header and
     * source, 44 of schema-1 numbers (nine floats, two uint32), 32 appended
     * by schema 2. */
    HK_CHECK_EQ_INT(sizeof(hk_profile_t), 116);
    HK_CHECK_EQ_INT(HK_PROFILE_SCHEMA, 2);

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

        /* A schema-1 blob is refused by name, not read with zeros in the new
         * fields. None was ever written to a device, so nothing migrates. */
        p = sample();
        p.schema = 1u;
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
    {   /* Either branch's release at zero is a gate, and each is named. */
        hk_profile_t p = sample();
        p.woofer_release_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_TIMING);
        p = sample();
        p.tweeter_release_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_TIMING);
        /* Hold may be zero: no hold is a choice, not a switch. */
        p = sample();
        p.woofer_hold_ms = 0u;
        p.tweeter_hold_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
    }
    {   /* Delay: bounded, and one branch only. */
        hk_profile_t p = sample();
        p.tweeter_delay_samples = HK_PROFILE_DELAY_MAX_SAMPLES; /* the edge is legal */
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
        p.tweeter_delay_samples = HK_PROFILE_DELAY_MAX_SAMPLES + 1u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_DELAY);

        p = sample();
        p.woofer_delay_samples = 65u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_DELAY);

        /* Both delayed is a latency wearing a correction's name. */
        p = sample();
        p.woofer_delay_samples = 1u;
        p.tweeter_delay_samples = 1u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_DELAY);

        p = sample();
        p.woofer_delay_samples = 12u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
    }
    {
        hk_profile_t p = sample();
        p.tweeter_polarity = 1u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
        p.tweeter_polarity = 2u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_POLARITY);
        p.tweeter_polarity = 0xFFFFFFFFu;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_POLARITY);
    }
    {   /* The adapter budget: absent, impossible, or more than two branches
         * can carry; and a window of zero. */
        hk_profile_t p = sample();
        p.supply_budget_sq = 0.0f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_BUDGET);
        p = sample();
        p.supply_budget_sq = NAN;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_BUDGET);
        p = sample();
        p.supply_budget_sq = -0.5f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_BUDGET);
        p = sample();
        p.supply_budget_sq = HK_PROFILE_SUPPLY_BUDGET_MAX; /* the edge is legal */
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
        p.supply_budget_sq = HK_PROFILE_SUPPLY_BUDGET_MAX + 0.5f;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_BUDGET);
        p = sample();
        p.supply_window_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_BUDGET);
    }
    {   /* Amplifier gain: unread is accepted, each strap setting is accepted,
         * anything else is a transcription error. */
        hk_profile_t p = sample();
        static const uint32_t settings[] = {0u, 20u, 26u, 32u, 36u};
        for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
            p.amp_gain_db = settings[i];
            HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_OK);
        }
        p.amp_gain_db = 30u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_AMP_GAIN);
        p.amp_gain_db = 1u;
        HK_CHECK_EQ_INT(hk_profile_valid(&p), HK_PROFILE_BAD_AMP_GAIN);
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
        HK_CHECK(close_to(read.supply_budget_sq, written.supply_budget_sq));
        HK_CHECK(read.tweeter_release_ms == written.tweeter_release_ms);
        HK_CHECK(strcmp(read.source, written.source) == 0);

        /* A blob of the wrong length is not an older profile, it is not one.
         * 84 bytes is exactly what a schema-1 struct was. */
        HK_CHECK_EQ_INT(hk_profile_from_blob(&written, sizeof(written) - 1u, &read),
                        HK_PROFILE_BAD_SCHEMA);
        HK_CHECK_EQ_INT(hk_profile_from_blob(&written, 84u, &read),
                        HK_PROFILE_BAD_SCHEMA);
        HK_CHECK_EQ_INT(hk_profile_from_blob(NULL, sizeof(written), &read),
                        HK_PROFILE_BAD_SCHEMA);

        /* A refusal must not leave a usable profile behind. */
        hk_profile_t broken = sample();
        broken.woofer_release_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_from_blob(&broken, sizeof(broken), &read),
                        HK_PROFILE_BAD_TIMING);
        HK_CHECK(read.schema == 0u && read.crossover_hz == 0.0f);
    }

    /* ===== the ceiling follows the supply in ONE direction ===== */
    {
        /* At the voltage it was measured at, it is itself. */
        HK_CHECK(close_to(hk_profile_ceiling_at(0.5f, 24000.0f, 24000.0f), 0.5f));

        /* A higher supply: the digital ceiling comes DOWN by the ratio. This
         * is the design case -- a bench profile listened to at 12 V meeting
         * the 24 V adapter -- and it errs quiet. It is asserted rather than
         * assumed. */
        HK_CHECK(hk_profile_ceiling_at(0.5f, 12000.0f, 24000.0f) < 0.5f);
        HK_CHECK(close_to(hk_profile_ceiling_at(0.5f, 12000.0f, 24000.0f),
                          0.5f * 12000.0f / 24000.0f));

        /* A lower supply does NOT raise it. The old rule doubled a ceiling
         * measured on the adapter when the chain was built for the 12 V
         * bench, on the premise that the driver's volts follow the rail; the
         * TPA3110D2 is fixed-gain (SLOS528F Table 3), so they do not, and the
         * raise was the one unsafe direction. The stored value is the
         * answer, exactly. */
        HK_CHECK(hk_profile_ceiling_at(0.4f, 24000.0f, 12000.0f) == 0.4f);
        HK_CHECK(hk_profile_ceiling_at(0.9f, 24000.0f, 12000.0f) == 0.9f);
        HK_CHECK(hk_profile_ceiling_at(1.0f, 24000.0f, 8000.0f) == 1.0f);

        /* Nonsense in, zero out -- and zero means do not play, not silence. */
        HK_CHECK(hk_profile_ceiling_at(0.5f, 24000.0f, 0.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.5f, 24000.0f, -1.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.0f, 24000.0f, 24000.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(0.5f, NAN, 24000.0f) == 0.0f);
        HK_CHECK(hk_profile_ceiling_at(1.5f, 24000.0f, 24000.0f) == 0.0f);
    }

    /* ===== building a chain ===== */
    {
        const hk_profile_t p = sample();
        hk_profile_chain_t chain;

        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 24000.0f, &chain), HK_PROFILE_OK);
        /* The subsonic filter is two sections now, both stable, and NOT an
         * LR4 pair: the sections differ. */
        HK_CHECK(hk_biquad_stable(&chain.woofer_hpf.section[0]));
        HK_CHECK(hk_biquad_stable(&chain.woofer_hpf.section[1]));
        HK_CHECK(chain.woofer_hpf.section[0].a1 != chain.woofer_hpf.section[1].a1);
        HK_CHECK(hk_biquad_stable(&chain.woofer_low.section[0]));
        HK_CHECK(hk_biquad_stable(&chain.woofer_low.section[1]));
        HK_CHECK(hk_biquad_stable(&chain.tweeter_high.section[0]));
        HK_CHECK(hk_biquad_stable(&chain.tweeter_high.section[1]));
        HK_CHECK(chain.woofer_limit.sample_rate == 44100u);
        HK_CHECK(chain.supply_limit.sample_rate == 44100u);

        /* Per-branch timing: each limiter carries its own branch's numbers. */
        HK_CHECK(chain.woofer_limit.release_ms == p.woofer_release_ms);
        HK_CHECK(chain.woofer_limit.hold_ms == p.woofer_hold_ms);
        HK_CHECK(chain.tweeter_limit.release_ms == p.tweeter_release_ms);
        HK_CHECK(chain.tweeter_limit.hold_ms == p.tweeter_hold_ms);
        HK_CHECK(chain.woofer_limit.release_ms != chain.tweeter_limit.release_ms);
        HK_CHECK(hk_limiter_config_valid(&chain.woofer_limit));
        HK_CHECK(hk_limiter_config_valid(&chain.tweeter_limit));
        HK_CHECK(hk_supply_limiter_config_valid(&chain.supply_limit));
        HK_CHECK(close_to(chain.woofer_gain, p.woofer_gain));
        HK_CHECK(chain.tweeter_sign == 1.0f);
        HK_CHECK(chain.woofer_delay_samples == 0u && chain.tweeter_delay_samples == 0u);

        /* The budget is carried as read: same numbers, no scaling. */
        HK_CHECK(close_to(chain.supply_limit.budget_sq, p.supply_budget_sq));
        HK_CHECK(chain.supply_limit.window_ms == p.supply_window_ms);

        /* The built ceiling is the scaled one, not the stored one. */
        HK_CHECK(close_to(chain.tweeter_limit.ceiling,
                          hk_profile_ceiling_at(p.tweeter_ceiling, p.reference_supply_mv,
                                                24000.0f)));

        /* Same profile, lower supply: the ceilings stay where they were
         * (one-directional), and so does everything else -- including the
         * budget, which describes the adapter and not this chain's supply. */
        hk_profile_chain_t low;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 12000.0f, &low), HK_PROFILE_OK);
        HK_CHECK(low.tweeter_limit.ceiling == chain.tweeter_limit.ceiling);
        HK_CHECK(low.woofer_limit.ceiling == chain.woofer_limit.ceiling);
        HK_CHECK(close_to(low.woofer_low.section[0].b0, chain.woofer_low.section[0].b0));
        HK_CHECK(close_to(low.supply_limit.budget_sq, chain.supply_limit.budget_sq));

        /* Same profile, higher supply than its reference: the ceilings move
         * down and only the ceilings. A profile referenced to 12 V is what a
         * bench profile is. */
        hk_profile_t bench = sample();
        bench.reference_supply_mv = 12000.0f;
        hk_profile_chain_t on_adapter;
        HK_CHECK_EQ_INT(hk_profile_build(&bench, 44100.0f, 24000.0f, &on_adapter), HK_PROFILE_OK);
        HK_CHECK(close_to(on_adapter.tweeter_limit.ceiling, bench.tweeter_ceiling * 0.5f));
        HK_CHECK(close_to(on_adapter.woofer_limit.ceiling, bench.woofer_ceiling * 0.5f));
        HK_CHECK(close_to(on_adapter.supply_limit.budget_sq, bench.supply_budget_sq));
        HK_CHECK(close_to(on_adapter.woofer_gain, bench.woofer_gain));

        /* Polarity and delay reach the chain as a sign and a count. */
        hk_profile_t aligned = sample();
        aligned.tweeter_polarity = 1u;
        aligned.tweeter_delay_samples = 9u;
        hk_profile_chain_t corrected;
        HK_CHECK_EQ_INT(hk_profile_build(&aligned, 44100.0f, 24000.0f, &corrected), HK_PROFILE_OK);
        HK_CHECK(corrected.tweeter_sign == -1.0f);
        HK_CHECK(corrected.tweeter_delay_samples == 9u);
        HK_CHECK(corrected.woofer_delay_samples == 0u);

        /* Both rates this product could run at build stable subsonic sections. */
        hk_profile_chain_t at48;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 48000.0f, 24000.0f, &at48), HK_PROFILE_OK);
        HK_CHECK(hk_biquad_stable(&at48.woofer_hpf.section[0]));
        HK_CHECK(hk_biquad_stable(&at48.woofer_hpf.section[1]));
        HK_CHECK(at48.woofer_limit.sample_rate == 48000u);
    }

    /* ===== a profile that cannot be built at this rate is refused, not moved ===== */
    {
        hk_profile_t p = sample();
        /* Above Nyquist the design has no meaning; silently pulling it down
         * would produce a crossover nobody chose. */
        p.crossover_hz = 30000.0f;
        hk_profile_chain_t chain;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 24000.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);
        /* And nothing half-built is left behind: the subsonic sections were
         * designed before the crossover refused, and they must not survive. */
        HK_CHECK(chain.woofer_hpf.section[0].b0 == 0.0f);
        HK_CHECK(chain.woofer_gain == 0.0f);

        p = sample();
        HK_CHECK_EQ_INT(hk_profile_build(&p, 0.0f, 24000.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 0.0f, &chain),
                        HK_PROFILE_UNBUILDABLE);
        HK_CHECK_EQ_INT(hk_profile_build(NULL, 44100.0f, 24000.0f, &chain),
                        HK_PROFILE_BAD_SCHEMA);
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 24000.0f, NULL),
                        HK_PROFILE_UNBUILDABLE);

        /* An invalid profile fails with its own reason, not a generic one. */
        p = sample();
        p.tweeter_gain = 2.0f;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 24000.0f, &chain),
                        HK_PROFILE_BAD_GAIN);
        p = sample();
        p.supply_window_ms = 0u;
        HK_CHECK_EQ_INT(hk_profile_build(&p, 44100.0f, 24000.0f, &chain),
                        HK_PROFILE_BAD_BUDGET);
    }

    /* ===== the one judge: bytes in, verdict out, both outputs or neither ===== */
    {
        const hk_profile_t written = sample();
        hk_profile_t profile;
        hk_profile_chain_t chain;

        /* Happy path: the same answer hk_profile_from_blob + hk_profile_build
         * would give, and both outputs filled. */
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written), 44100.0f, 24000.0f,
                                        &profile, &chain), HK_PROFILE_OK);
        HK_CHECK(close_to(profile.crossover_hz, written.crossover_hz));
        HK_CHECK(strcmp(profile.source, written.source) == 0);
        HK_CHECK(hk_biquad_stable(&chain.tweeter_high.section[1]));
        HK_CHECK(chain.woofer_limit.sample_rate == 44100u);
        hk_profile_chain_t direct;
        HK_CHECK_EQ_INT(hk_profile_build(&written, 44100.0f, 24000.0f, &direct), HK_PROFILE_OK);
        HK_CHECK(memcmp(&direct, &chain, sizeof(direct)) == 0);

        /* Verdict only: NULL outputs are legal, and it is the same verdict. */
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written), 44100.0f, 24000.0f,
                                        NULL, NULL), HK_PROFILE_OK);
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written), 44100.0f, 24000.0f,
                                        &profile, NULL), HK_PROFILE_OK);
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written), 44100.0f, 24000.0f,
                                        NULL, &chain), HK_PROFILE_OK);

        /* A short blob: BAD_SCHEMA, and both outputs zeroed. */
        memset(&profile, 0x5A, sizeof(profile));
        memset(&chain, 0x5A, sizeof(chain));
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written) - 4u, 44100.0f, 24000.0f,
                                        &profile, &chain), HK_PROFILE_BAD_SCHEMA);
        HK_CHECK(profile.schema == 0u && profile.crossover_hz == 0.0f);
        HK_CHECK(chain.woofer_gain == 0.0f && chain.woofer_limit.sample_rate == 0u);
        HK_CHECK_EQ_INT(hk_profile_load(NULL, sizeof(written), 44100.0f, 24000.0f,
                                        &profile, &chain), HK_PROFILE_BAD_SCHEMA);

        /* A structurally bad blob: its own verdict, both outputs zeroed. */
        hk_profile_t bad = sample();
        bad.tweeter_polarity = 3u;
        memset(&profile, 0x5A, sizeof(profile));
        memset(&chain, 0x5A, sizeof(chain));
        HK_CHECK_EQ_INT(hk_profile_load(&bad, sizeof(bad), 44100.0f, 24000.0f,
                                        &profile, &chain), HK_PROFILE_BAD_POLARITY);
        HK_CHECK(profile.schema == 0u);
        HK_CHECK(chain.tweeter_sign == 0.0f);

        /* Valid bytes that will not build at this rate: UNBUILDABLE, and the
         * PROFILE is zeroed too, even though it read fine -- a caller must
         * not be left holding a profile next to no chain. */
        hk_profile_t high = sample();
        high.crossover_hz = 30000.0f;
        memset(&profile, 0x5A, sizeof(profile));
        HK_CHECK_EQ_INT(hk_profile_load(&high, sizeof(high), 44100.0f, 24000.0f,
                                        &profile, &chain), HK_PROFILE_UNBUILDABLE);
        HK_CHECK(profile.schema == 0u && profile.crossover_hz == 0.0f);
        HK_CHECK(chain.woofer_gain == 0.0f);

        /* Verdict-only refusals still report the right reason. */
        HK_CHECK_EQ_INT(hk_profile_load(&high, sizeof(high), 44100.0f, 24000.0f,
                                        NULL, NULL), HK_PROFILE_UNBUILDABLE);
        HK_CHECK_EQ_INT(hk_profile_load(&bad, sizeof(bad), 44100.0f, 24000.0f,
                                        NULL, NULL), HK_PROFILE_BAD_POLARITY);
        HK_CHECK_EQ_INT(hk_profile_load(&written, sizeof(written), 0.0f, 24000.0f,
                                        NULL, NULL), HK_PROFILE_UNBUILDABLE);
    }

    /* ===== every verdict has a name, so a log line can carry it ===== */
    for (int v = HK_PROFILE_OK; v <= HK_PROFILE_BAD_AMP_GAIN; v++) {
        const char *name = hk_profile_verdict_name((hk_profile_verdict_t)v);
        HK_CHECK(name != NULL && strcmp(name, "unknown") != 0);
    }
    HK_CHECK_EQ_STR(hk_profile_verdict_name(HK_PROFILE_BAD_DELAY), "delay");
    HK_CHECK_EQ_STR(hk_profile_verdict_name(HK_PROFILE_BAD_POLARITY), "polarity");
    HK_CHECK_EQ_STR(hk_profile_verdict_name(HK_PROFILE_BAD_BUDGET), "budget");
    HK_CHECK_EQ_STR(hk_profile_verdict_name(HK_PROFILE_BAD_AMP_GAIN), "amp-gain");
}
