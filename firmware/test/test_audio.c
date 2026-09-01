#include "hk_test.h"
#include "hk_audio.h"

#include <string.h>

/*
 * The sequence protects two things that cannot be repaired in software: a
 * tweeter whose impedance has not been measured, and the listener's ears. So
 * the tests are mostly about ORDER, not about timing — a fast sequence in the
 * wrong order is worse than a slow one in the right order.
 */

static hk_audio_timing_t timing(void)
{
    /* Stand-ins. The real settle times come from watching the rails on a scope
     * at G1/G3 and do not exist yet. */
    hk_audio_timing_t t = {.clock_settle_ms = 50, .dac_settle_ms = 100,
                           .mute_settle_ms = 20};
    return t;
}

static hk_audio_inputs_t run(uint32_t now_ms)
{
    hk_audio_inputs_t i = {.permitted = true, .stream_live = true, .now_ms = now_ms};
    return i;
}

/** Advance to @p now_ms with the given wishes. */
static void tick(hk_audio_t *seq, bool permitted, bool live, uint32_t now_ms)
{
    hk_audio_timing_t t = timing();
    hk_audio_inputs_t in = {.permitted = permitted, .stream_live = live, .now_ms = now_ms};
    hk_audio_step(seq, &in, &t);
}

void test_audio(void)
{
    hk_audio_t seq;
    hk_audio_timing_t t = timing();
    hk_audio_inputs_t in;

    /* ===== the invariants, over every state that exists =====
     * These are the reason the module is shaped this way, so they are checked
     * exhaustively rather than along one happy path. A state added later that
     * breaks the ordering fails here without anyone having to remember why. */
    {
        const hk_audio_state_t all[] = {HK_AUDIO_SILENT, HK_AUDIO_CLOCKING,
                                        HK_AUDIO_DAC_LIVE, HK_AUDIO_PLAYING,
                                        HK_AUDIO_MUTING};
        for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
            hk_audio_outputs_t o = hk_audio_outputs(all[i]);

            /* The amplifier is never live while the DAC is muted: whatever the
             * DAC does on its first samples would be multiplied by the
             * amplifier's gain. */
            HK_CHECK(!o.amp_enabled || o.dac_unmuted);

            /* The DAC is never unmuted without a clock. Unmuting into an
             * absent or settling bit clock puts a step on its output. */
            HK_CHECK(!o.dac_unmuted || o.i2s_running);

            /* And the amplifier is live in exactly one state. */
            HK_CHECK(!o.amp_enabled || all[i] == HK_AUDIO_PLAYING);
        }
    }

    /* MUTING is the asymmetric one: amplifier already down, everything else
     * still up. If this ever matched PLAYING or SILENT the state would be
     * pointless and the shutdown order would be lost. */
    {
        hk_audio_outputs_t m = hk_audio_outputs(HK_AUDIO_MUTING);
        HK_CHECK(m.i2s_running && m.dac_unmuted && !m.amp_enabled);
    }

    /* ===== bringing sound up ===== */
    hk_audio_init(&seq, 1000);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);

    tick(&seq, true, true, 1000);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_CLOCKING);

    tick(&seq, true, true, 1049);            /* one short of the clock settle */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_CLOCKING);
    tick(&seq, true, true, 1050);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_DAC_LIVE);

    tick(&seq, true, true, 1149);            /* one short of the DAC settle */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_DAC_LIVE);
    tick(&seq, true, true, 1150);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_PLAYING);
    HK_CHECK(hk_audio_outputs(seq.state).amp_enabled);

    /* ===== taking it down: the amplifier goes first ===== */
    tick(&seq, true, false, 2000);           /* stream stopped */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_MUTING);
    {
        hk_audio_outputs_t o = hk_audio_outputs(seq.state);
        HK_CHECK(!o.amp_enabled);            /* down immediately */
        HK_CHECK(o.dac_unmuted);             /* and still ahead of the DAC */
        HK_CHECK(o.i2s_running);
    }
    tick(&seq, true, false, 2019);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_MUTING);
    tick(&seq, true, false, 2020);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);

    /* ===== losing permission mid-play is the same path ===== */
    hk_audio_init(&seq, 0);
    tick(&seq, true, true, 0);
    tick(&seq, true, true, 50);
    tick(&seq, true, true, 150);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_PLAYING);
    tick(&seq, false, true, 200);            /* pack went flat, or charging started */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_MUTING);
    HK_CHECK(!hk_audio_outputs(seq.state).amp_enabled);

    /* ===== an unwind runs to completion =====
     * Turning back here would release the amplifier while the DAC is part way
     * through its own transition. A fresh start costs one settle time. */
    tick(&seq, true, true, 205);             /* permission returns immediately */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_MUTING);
    tick(&seq, true, true, 220);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);
    tick(&seq, true, true, 221);             /* and only now does it restart */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_CLOCKING);

    /* ===== giving up before the amplifier was ever live ===== */
    /* From CLOCKING nothing is unmuted, so the clocks may simply stop. */
    hk_audio_init(&seq, 0);
    tick(&seq, true, true, 0);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_CLOCKING);
    tick(&seq, true, false, 10);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);

    /* From DAC_LIVE the DAC is up, so it unwinds properly. */
    hk_audio_init(&seq, 0);
    tick(&seq, true, true, 0);
    tick(&seq, true, true, 50);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_DAC_LIVE);
    tick(&seq, false, true, 60);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_MUTING);

    /* ===== never allowed means never started ===== */
    hk_audio_init(&seq, 0);
    for (uint32_t ms = 0; ms < 5000; ms += 100) {
        tick(&seq, false, true, ms);
        HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);
    }
    /* A live stream with no permission must not creep up either. */
    hk_audio_init(&seq, 0);
    for (uint32_t ms = 0; ms < 5000; ms += 100) {
        tick(&seq, true, false, ms);
        HK_CHECK(!hk_audio_outputs(seq.state).amp_enabled);
    }

    /* ===== missing information takes the safe path ===== */
    hk_audio_init(&seq, 0);
    tick(&seq, true, true, 0);
    tick(&seq, true, true, 50);
    tick(&seq, true, true, 150);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_PLAYING);
    hk_audio_step(&seq, NULL, &t);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);

    hk_audio_init(&seq, 0);
    tick(&seq, true, true, 0);
    in = run(0);
    hk_audio_step(&seq, &in, NULL);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);

    hk_audio_step(NULL, &in, &t);            /* must not crash */

    /* ===== the millisecond counter wraps ===== */
    /* A speaker left on for 49.7 days must not stall halfway through a
     * transition because the clock rolled over. */
    hk_audio_init(&seq, 0xFFFFFFF0u);
    tick(&seq, true, true, 0xFFFFFFF0u);
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_CLOCKING);
    tick(&seq, true, true, 0x00000022u);     /* 50 ms later, across the wrap */
    HK_CHECK_EQ_INT(seq.state, HK_AUDIO_DAC_LIVE);

    /* ===== a hundred cycles leave no residue ===== */
    hk_audio_init(&seq, 0);
    {
        uint32_t ms = 0;
        for (int cycle = 0; cycle < 100; cycle++) {
            tick(&seq, true, true, ms);        ms += 60;
            tick(&seq, true, true, ms);        ms += 110;
            tick(&seq, true, true, ms);
            HK_CHECK_EQ_INT(seq.state, HK_AUDIO_PLAYING);
            ms += 10;
            tick(&seq, true, false, ms);       ms += 30;
            tick(&seq, true, false, ms);
            HK_CHECK_EQ_INT(seq.state, HK_AUDIO_SILENT);
            ms += 10;
        }
    }

    /* ===== names ===== */
    HK_CHECK(strcmp(hk_audio_state_name(HK_AUDIO_MUTING), "MUTING") == 0);
    HK_CHECK(strcmp(hk_audio_state_name(HK_AUDIO_PLAYING), "PLAYING") == 0);
    HK_CHECK(strcmp(hk_audio_state_name((hk_audio_state_t)99), "INVALID") == 0);
}
