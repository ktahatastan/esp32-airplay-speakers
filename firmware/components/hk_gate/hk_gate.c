#include "hk_gate.h"

#include <stddef.h>

hk_gate_result_t hk_gate_evaluate(const hk_gate_inputs_t *inputs,
                                  const hk_gate_limits_t *limits)
{
    if (inputs == NULL) {
        return HK_GATE_NO_LIMITS;
    }
    if (limits == NULL) {
        /* No calibrated thresholds means there is nothing to judge the readings
         * against. The plan is explicit: without the measurement, no update. */
        return HK_GATE_NO_LIMITS;
    }

    if (inputs->update_in_progress) {
        return HK_GATE_UPDATE_IN_PROGRESS;
    }
    if (!inputs->wifi_connected) {
        return HK_GATE_NO_WIFI;
    }
    /* Checked before power, because it is the one the owner would actually
     * notice: nothing about this update is worth interrupting a song for. */
    if (inputs->audio_active) {
        return HK_GATE_AUDIO_ACTIVE;
    }

    if (inputs->charging) {
        /* External power is present, so pack voltage stops being the question.
         * This is also the best moment to update: ADR-0004 keeps the amplifier
         * off while charging, so the speaker is already idle. */
        return HK_GATE_GO;
    }

    if (inputs->battery_mv == HK_GATE_BATTERY_UNKNOWN) {
        return HK_GATE_BATTERY_UNREAD;
    }
    if (inputs->battery_mv < limits->min_battery_mv) {
        return HK_GATE_BATTERY_LOW;
    }

    if (inputs->temperature_c == HK_GATE_TEMPERATURE_UNKNOWN) {
        return HK_GATE_TEMPERATURE_UNREAD;
    }
    if (inputs->temperature_c > limits->max_temperature_c) {
        return HK_GATE_TEMPERATURE_HIGH;
    }

    return HK_GATE_GO;
}

bool hk_gate_retry_soon(hk_gate_result_t result)
{
    switch (result) {
    case HK_GATE_AUDIO_ACTIVE:
    case HK_GATE_UPDATE_IN_PROGRESS:
    case HK_GATE_NO_WIFI:
    case HK_GATE_TEMPERATURE_HIGH:
        /* All of these pass on their own: a track ends, a download finishes,
         * a router comes back, a pack cools down. */
        return true;
    case HK_GATE_BATTERY_LOW:
        /* Worth another look: the owner may plug it in. */
        return true;
    case HK_GATE_GO:
    case HK_GATE_NO_LIMITS:
    case HK_GATE_BATTERY_UNREAD:
    case HK_GATE_TEMPERATURE_UNREAD:
    default:
        /* A missing sensor or missing calibration is a standing condition, not
         * a passing one. Polling it every few minutes burns power to learn
         * nothing. */
        return false;
    }
}

const char *hk_gate_result_name(hk_gate_result_t result)
{
    switch (result) {
    case HK_GATE_GO:                 return "go";
    case HK_GATE_NO_LIMITS:          return "no_calibrated_limits";
    case HK_GATE_UPDATE_IN_PROGRESS: return "update_in_progress";
    case HK_GATE_NO_WIFI:            return "no_wifi";
    case HK_GATE_AUDIO_ACTIVE:       return "audio_active";
    case HK_GATE_BATTERY_UNREAD:     return "battery_unread";
    case HK_GATE_BATTERY_LOW:        return "battery_low";
    case HK_GATE_TEMPERATURE_UNREAD: return "temperature_unread";
    case HK_GATE_TEMPERATURE_HIGH:   return "temperature_high";
    }
    return "unknown";
}
