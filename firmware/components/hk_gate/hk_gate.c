#include "hk_gate.h"

#include <stddef.h>

hk_gate_result_t hk_gate_evaluate(const hk_gate_inputs_t *inputs)
{
    if (inputs == NULL) {
        return HK_GATE_NO_INPUTS;
    }

    if (inputs->update_in_progress) {
        return HK_GATE_UPDATE_IN_PROGRESS;
    }
    if (!inputs->wifi_connected) {
        return HK_GATE_NO_WIFI;
    }
    /* Checked last, because it is the one the owner would actually notice:
     * nothing about this update is worth interrupting a song for. */
    if (inputs->audio_active) {
        return HK_GATE_AUDIO_ACTIVE;
    }

    return HK_GATE_GO;
}

bool hk_gate_retry_soon(hk_gate_result_t result)
{
    switch (result) {
    case HK_GATE_AUDIO_ACTIVE:
    case HK_GATE_UPDATE_IN_PROGRESS:
    case HK_GATE_NO_WIFI:
        /* All of these pass on their own: a track ends, a download finishes,
         * a router comes back. */
        return true;
    case HK_GATE_GO:
    case HK_GATE_NO_INPUTS:
    default:
        /* A caller with nothing to say is a standing condition, not a passing
         * one. Polling it every few minutes burns power to learn nothing. */
        return false;
    }
}

const char *hk_gate_result_name(hk_gate_result_t result)
{
    switch (result) {
    case HK_GATE_GO:                 return "go";
    case HK_GATE_NO_INPUTS:          return "no_inputs";
    case HK_GATE_UPDATE_IN_PROGRESS: return "update_in_progress";
    case HK_GATE_NO_WIFI:            return "no_wifi";
    case HK_GATE_AUDIO_ACTIVE:       return "audio_active";
    }
    return "unknown";
}
