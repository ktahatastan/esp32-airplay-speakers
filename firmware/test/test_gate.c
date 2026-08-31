#include "hk_test.h"
#include "hk_gate.h"

/*
 * The gate table in docs/03-firmware/ota-and-release-plan.md.
 *
 * The rule under test is that an unknown reading blocks. There is no default
 * threshold in the module, so a device without calibration cannot update at
 * all — the same conclusion hk_storage reaches about audio, for the same
 * reason: a number nobody measured looks exactly like a measured one.
 */

static hk_gate_limits_t limits(void)
{
    /* Stand-ins for this test only. The real values come from the calibration
     * store once G3 and G4 have produced them. */
    hk_gate_limits_t l = {.min_battery_mv = 14000, .max_temperature_c = 45};
    return l;
}

static hk_gate_inputs_t ready(void)
{
    hk_gate_inputs_t i = {
        .audio_active = false,
        .wifi_connected = true,
        .update_in_progress = false,
        .charging = false,
        .battery_mv = 16000,
        .temperature_c = 25,
    };
    return i;
}

void test_gate(void)
{
    hk_gate_limits_t lim = limits();
    hk_gate_inputs_t in;

    /* --- a device in a fit state --- */
    in = ready();
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_GO);

    /* --- without calibrated thresholds there is nothing to judge against --- */
    in = ready();
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, NULL), HK_GATE_NO_LIMITS);
    HK_CHECK_EQ_INT(hk_gate_evaluate(NULL, &lim), HK_GATE_NO_LIMITS);

    /* --- one blocker at a time --- */
    in = ready(); in.update_in_progress = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_UPDATE_IN_PROGRESS);
    in = ready(); in.wifi_connected = false;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_NO_WIFI);
    in = ready(); in.audio_active = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_AUDIO_ACTIVE);
    in = ready(); in.battery_mv = 13999;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_BATTERY_LOW);
    in = ready(); in.temperature_c = 46;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_TEMPERATURE_HIGH);

    /* --- an unread sensor blocks, exactly like a bad reading --- */
    in = ready(); in.battery_mv = HK_GATE_BATTERY_UNKNOWN;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_BATTERY_UNREAD);
    in = ready(); in.temperature_c = HK_GATE_TEMPERATURE_UNKNOWN;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_TEMPERATURE_UNREAD);

    /* --- boundaries are inclusive where the wording says "below" and "above" --- */
    in = ready(); in.battery_mv = lim.min_battery_mv;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_GO);
    in = ready(); in.temperature_c = lim.max_temperature_c;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_GO);

    /* --- external power settles the power question --- */
    in = ready(); in.charging = true; in.battery_mv = 12000;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_GO);
    /* Even an unread pack, because the pack is not what is powering it. */
    in = ready(); in.charging = true; in.battery_mv = HK_GATE_BATTERY_UNKNOWN;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_GO);
    /* But charging does not excuse playback or a missing network. */
    in = ready(); in.charging = true; in.audio_active = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_AUDIO_ACTIVE);
    in = ready(); in.charging = true; in.wifi_connected = false;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_NO_WIFI);

    /* --- playback outranks power, because it is what the owner would notice --- */
    in = ready(); in.audio_active = true; in.battery_mv = 12000;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in, &lim), HK_GATE_AUDIO_ACTIVE);

    /* --- what is worth trying again shortly --- */
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_AUDIO_ACTIVE), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_NO_WIFI), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_BATTERY_LOW), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_TEMPERATURE_HIGH), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_UPDATE_IN_PROGRESS), 1);
    /* A missing sensor or missing calibration will still be missing in five
     * minutes; polling it only burns power. */
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_NO_LIMITS), 0);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_BATTERY_UNREAD), 0);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_TEMPERATURE_UNREAD), 0);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_GO), 0);

    HK_CHECK_EQ_STR(hk_gate_result_name(HK_GATE_NO_LIMITS), "no_calibrated_limits");
}
