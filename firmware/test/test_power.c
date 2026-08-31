#include "hk_test.h"
#include "hk_power.h"

#include <string.h>

/*
 * Stand-in numbers for this test only. The real ones come from G3/G4 and do
 * not exist yet; nothing here should be read as a calibrated threshold. A 4S
 * Li-ion pack runs roughly 12.0-16.8 V, so these are plausible shapes rather
 * than measurements.
 */
static hk_power_limits_t limits(void)
{
    hk_power_limits_t l = {
        .implausible_low_mv = 8000,
        .implausible_high_mv = 18000,
        .low_mv = 13600,
        .critical_mv = 12800,
        .shutdown_mv = 12000,
        .recover_margin_mv = 300,
        .max_cell_c = 45,
    };
    return l;
}

static hk_power_inputs_t at(uint32_t mv)
{
    hk_power_inputs_t i = {.pack_mv = mv, .cell_c = 25, .charging = false};
    return i;
}

static hk_power_state_t step(hk_power_state_t prev, hk_power_inputs_t in)
{
    hk_power_limits_t lim = limits();
    return hk_power_evaluate(prev, &in, &lim);
}

void test_power(void)
{
    hk_power_limits_t lim = limits();
    hk_power_inputs_t in;

    /* ================= limits must make sense ================= */
    HK_CHECK(hk_power_limits_sane(&lim));
    HK_CHECK(!hk_power_limits_sane(NULL));

    {   /* thresholds out of order cannot express what they ask for */
        hk_power_limits_t bad = limits();
        bad.critical_mv = bad.low_mv + 100;
        HK_CHECK(!hk_power_limits_sane(&bad));

        bad = limits();
        bad.shutdown_mv = bad.critical_mv;
        HK_CHECK(!hk_power_limits_sane(&bad));

        /* a threshold below what the sensor can report never fires */
        bad = limits();
        bad.shutdown_mv = bad.implausible_low_mv;
        HK_CHECK(!hk_power_limits_sane(&bad));

        bad = limits();
        bad.low_mv = bad.implausible_high_mv;
        HK_CHECK(!hk_power_limits_sane(&bad));

        /* an inverted bracket is not a bracket */
        bad = limits();
        bad.implausible_low_mv = bad.implausible_high_mv;
        HK_CHECK(!hk_power_limits_sane(&bad));

        /* recovery with no margin is the oscillation this module avoids */
        bad = limits();
        bad.recover_margin_mv = 0;
        HK_CHECK(!hk_power_limits_sane(&bad));

        /* and unusable limits must not produce a confident verdict */
        in = at(16000);
        HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &bad), HK_POWER_UNKNOWN);
    }

    /* ================= plain classification ================= */
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(16000)), HK_POWER_NORMAL);
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(13000)), HK_POWER_LOW);
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(12500)), HK_POWER_CRITICAL);
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(11500)), HK_POWER_SHUTDOWN);

    /* boundaries belong to the better side: "below low_mv" means below */
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(13600)), HK_POWER_NORMAL);
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(13599)), HK_POWER_LOW);

    /* ================= an impossible reading is a sensor fault ================= */
    /* This is the distinction the module exists for. A 4S pack cannot read
     * zero, and calling that SHUTDOWN would turn a loose divider into a
     * speaker that switches itself off mid-song. */
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(0)), HK_POWER_UNKNOWN);   /* 0 is the unread sentinel */
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(4000)), HK_POWER_SENSOR_FAULT);
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(25000)), HK_POWER_SENSOR_FAULT);
    /* and it must not be reported as a flat battery */
    HK_CHECK(step(HK_POWER_NORMAL, at(4000)) != HK_POWER_SHUTDOWN);

    /* exactly on the bracket counts as implausible */
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(8000)), HK_POWER_SENSOR_FAULT);
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(18000)), HK_POWER_SENSOR_FAULT);

    /* ================= an unread pack is judged as nothing ================= */
    in = at(HK_POWER_MV_UNKNOWN);
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_UNKNOWN);
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, NULL, &lim), HK_POWER_UNKNOWN);
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, NULL), HK_POWER_UNKNOWN);

    /* ================= temperature outranks voltage ================= */
    in = at(16000);
    in.cell_c = 60;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_OVERHEAT);
    /* even on a full pack, and even while charging */
    in.charging = true;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_OVERHEAT);
    /* at the limit is still fine; above it is not */
    in = at(16000); in.cell_c = 45;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_NORMAL);
    in.cell_c = 46;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_OVERHEAT);
    /* an unread thermistor is not an overheat */
    in = at(16000); in.cell_c = HK_POWER_C_UNKNOWN;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_NORMAL, &in, &lim), HK_POWER_NORMAL);

    /* ================= worse is immediate, better is earned ================= */
    /* Falling needs no margin: a flat pack does not wait. */
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(13000)), HK_POWER_LOW);
    HK_CHECK_EQ_INT(step(HK_POWER_LOW, at(12500)), HK_POWER_CRITICAL);
    HK_CHECK_EQ_INT(step(HK_POWER_NORMAL, at(11000)), HK_POWER_SHUTDOWN);  /* skips states */

    /* Climbing back does. This is the case a plain comparison gets wrong: a
     * pack sags while a note plays and recovers between notes, and the state
     * would flicker through the warnings all evening. */
    HK_CHECK_EQ_INT(step(HK_POWER_LOW, at(13650)), HK_POWER_LOW);       /* over the line, under the margin */
    HK_CHECK_EQ_INT(step(HK_POWER_LOW, at(13899)), HK_POWER_LOW);       /* one millivolt short */
    HK_CHECK_EQ_INT(step(HK_POWER_LOW, at(13900)), HK_POWER_NORMAL);    /* 13600 + 300 */

    HK_CHECK_EQ_INT(step(HK_POWER_CRITICAL, at(13000)), HK_POWER_CRITICAL); /* would be LOW without hysteresis */
    HK_CHECK_EQ_INT(step(HK_POWER_CRITICAL, at(13100)), HK_POWER_LOW);      /* 12800 + 300 */

    HK_CHECK_EQ_INT(step(HK_POWER_SHUTDOWN, at(12100)), HK_POWER_SHUTDOWN);
    HK_CHECK_EQ_INT(step(HK_POWER_SHUTDOWN, at(12300)), HK_POWER_CRITICAL);

    /* staying put is staying put */
    HK_CHECK_EQ_INT(step(HK_POWER_LOW, at(13000)), HK_POWER_LOW);

    /* ================= charging needs no hysteresis ================= */
    /* The pack is being filled, so a rising reading is rising for a real
     * reason rather than because the load let go. */
    in = at(13650); in.charging = true;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_LOW, &in, &lim), HK_POWER_NORMAL);
    in = at(12900); in.charging = true;
    HK_CHECK_EQ_INT(hk_power_evaluate(HK_POWER_SHUTDOWN, &in, &lim), HK_POWER_LOW);

    /* ================= recovering from a non-band state ================= */
    /* A sensor that just started working, or a pack that just cooled, has no
     * previous band to hold on to. */
    HK_CHECK_EQ_INT(step(HK_POWER_SENSOR_FAULT, at(13000)), HK_POWER_LOW);
    HK_CHECK_EQ_INT(step(HK_POWER_OVERHEAT, at(13000)), HK_POWER_LOW);
    HK_CHECK_EQ_INT(step(HK_POWER_UNKNOWN, at(13000)), HK_POWER_LOW);

    /* ================= audio permission ================= */
    /* ADR-0004: not while charging, whatever the voltage says. */
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_NORMAL, true));
    HK_CHECK(hk_power_audio_permitted(HK_POWER_NORMAL, false));

    /* A warning is a warning, not a mute. Cutting audio at the first warning
     * would make the speaker useless long before the pack is empty. */
    HK_CHECK(hk_power_audio_permitted(HK_POWER_LOW, false));

    /* Everything else refuses, including the two "we do not know" states: a
     * pack nobody can measure is not a pack known to be fine. */
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_CRITICAL, false));
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_SHUTDOWN, false));
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_UNKNOWN, false));
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_SENSOR_FAULT, false));
    HK_CHECK(!hk_power_audio_permitted(HK_POWER_OVERHEAT, false));

    /* ================= a discharge, start to finish ================= */
    {
        const uint32_t curve[] = {16800, 15000, 14000, 13500, 13100, 12900,
                                  12700, 12400, 11900};
        const hk_power_state_t want[] = {
            HK_POWER_NORMAL, HK_POWER_NORMAL, HK_POWER_NORMAL, HK_POWER_LOW,
            HK_POWER_LOW, HK_POWER_LOW, HK_POWER_CRITICAL, HK_POWER_CRITICAL,
            HK_POWER_SHUTDOWN,
        };
        hk_power_state_t s = HK_POWER_UNKNOWN;
        for (unsigned i = 0; i < sizeof(curve) / sizeof(curve[0]); i++) {
            s = step(s, at(curve[i]));
            HK_CHECK_EQ_INT(s, want[i]);
        }
        /* and it never silently improves on the way down */
        HK_CHECK_EQ_INT(s, HK_POWER_SHUTDOWN);
    }

    /* ================= names ================= */
    HK_CHECK(strcmp(hk_power_state_name(HK_POWER_SENSOR_FAULT), "SENSOR_FAULT") == 0);
    HK_CHECK(strcmp(hk_power_state_name(HK_POWER_OVERHEAT), "OVERHEAT") == 0);
    HK_CHECK(strcmp(hk_power_state_name((hk_power_state_t)99), "INVALID") == 0);
}
