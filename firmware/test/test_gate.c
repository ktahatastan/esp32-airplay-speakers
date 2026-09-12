#include "hk_test.h"
#include "hk_gate.h"

/*
 * The gate table in docs/03-firmware/ota-and-release-plan.md.
 *
 * The rule under test is that an unknown state blocks. A caller that offers
 * no inputs gets a refusal rather than a default, because a device that
 * cannot say what it is doing is not a device that should start rewriting its
 * flash.
 */

static hk_gate_inputs_t ready(void)
{
    hk_gate_inputs_t i = {
        .audio_active = false,
        .wifi_connected = true,
        .update_in_progress = false,
    };
    return i;
}

void test_gate(void)
{
    hk_gate_inputs_t in;

    /* --- a device in a fit state --- */
    in = ready();
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_GO);

    /* --- without inputs there is nothing to judge --- */
    HK_CHECK_EQ_INT(hk_gate_evaluate(NULL), HK_GATE_NO_INPUTS);

    /* --- one blocker at a time --- */
    in = ready(); in.update_in_progress = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_UPDATE_IN_PROGRESS);
    in = ready(); in.wifi_connected = false;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_NO_WIFI);
    in = ready(); in.audio_active = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_AUDIO_ACTIVE);

    /* --- ordering: the reason reported is the most decisive one --- */
    /* An update already running outranks everything: a second attempt on top
     * of it is the one thing that can corrupt the inactive slot. */
    in = ready(); in.update_in_progress = true; in.wifi_connected = false;
    in.audio_active = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_UPDATE_IN_PROGRESS);
    /* No network before playback: without Wi-Fi there is nothing to fetch,
     * whatever the speaker is doing. */
    in = ready(); in.wifi_connected = false; in.audio_active = true;
    HK_CHECK_EQ_INT(hk_gate_evaluate(&in), HK_GATE_NO_WIFI);

    /* --- what is worth trying again shortly --- */
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_AUDIO_ACTIVE), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_NO_WIFI), 1);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_UPDATE_IN_PROGRESS), 1);
    /* A caller with nothing to say will have nothing to say in five minutes
     * either; polling it only burns power. */
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_NO_INPUTS), 0);
    HK_CHECK_EQ_INT(hk_gate_retry_soon(HK_GATE_GO), 0);

    HK_CHECK_EQ_STR(hk_gate_result_name(HK_GATE_NO_INPUTS), "no_inputs");
    HK_CHECK_EQ_STR(hk_gate_result_name(HK_GATE_GO), "go");
}
