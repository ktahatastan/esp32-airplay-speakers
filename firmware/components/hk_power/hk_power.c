#include "hk_power.h"

#include <stddef.h>

bool hk_power_limits_sane(const hk_power_limits_t *limits)
{
    if (limits == NULL) {
        return false;
    }
    /* The plausible bracket has to be a bracket. */
    if (limits->implausible_low_mv >= limits->implausible_high_mv) {
        return false;
    }
    /* Worse thresholds must sit below better ones, or "below critical" and
     * "below low" describe overlapping nonsense. */
    if (!(limits->shutdown_mv < limits->critical_mv &&
          limits->critical_mv < limits->low_mv)) {
        return false;
    }
    /* Every threshold has to be inside the range the sensor can report, or it
     * can never be reached and the warning it guards never fires. */
    if (limits->shutdown_mv <= limits->implausible_low_mv ||
        limits->low_mv >= limits->implausible_high_mv) {
        return false;
    }
    /* Recovery with no margin is the oscillation this module exists to avoid. */
    if (limits->recover_margin_mv == 0u) {
        return false;
    }
    return true;
}

/** Where a believable voltage sits, ignoring hysteresis. */
static hk_power_state_t band_for(uint32_t mv, const hk_power_limits_t *limits)
{
    if (mv < limits->shutdown_mv) {
        return HK_POWER_SHUTDOWN;
    }
    if (mv < limits->critical_mv) {
        return HK_POWER_CRITICAL;
    }
    if (mv < limits->low_mv) {
        return HK_POWER_LOW;
    }
    return HK_POWER_NORMAL;
}

/** The floor a reading must clear to leave @p state for something better. */
static uint32_t recover_above(hk_power_state_t state, const hk_power_limits_t *limits)
{
    switch (state) {
    case HK_POWER_SHUTDOWN: return limits->shutdown_mv + limits->recover_margin_mv;
    case HK_POWER_CRITICAL: return limits->critical_mv + limits->recover_margin_mv;
    case HK_POWER_LOW:      return limits->low_mv + limits->recover_margin_mv;
    default:                return 0u;
    }
}

hk_power_state_t hk_power_evaluate(hk_power_state_t previous,
                                   const hk_power_inputs_t *inputs,
                                   const hk_power_limits_t *limits)
{
    if (inputs == NULL || !hk_power_limits_sane(limits)) {
        return HK_POWER_UNKNOWN;
    }

    /* Temperature outranks voltage. A hot pack is a hazard whatever its charge
     * state, and it is the one condition where continuing to draw current is
     * the wrong answer even on a full battery. */
    if (inputs->cell_c != HK_POWER_C_UNKNOWN && inputs->cell_c > limits->max_cell_c) {
        return HK_POWER_OVERHEAT;
    }

    if (inputs->pack_mv == HK_POWER_MV_UNKNOWN) {
        return HK_POWER_UNKNOWN;
    }

    /* A number outside what a 4S pack can produce says the sensor is wrong,
     * not the battery. Reporting it as SHUTDOWN would turn a loose divider
     * into a speaker that switches itself off. */
    if (inputs->pack_mv <= limits->implausible_low_mv ||
        inputs->pack_mv >= limits->implausible_high_mv) {
        return HK_POWER_SENSOR_FAULT;
    }

    const hk_power_state_t band = band_for(inputs->pack_mv, limits);

    /* Charging only ever improves the situation, so there is nothing to hold
     * on to: the pack is being filled and the reading is rising for a real
     * reason rather than because the load let go. */
    if (inputs->charging) {
        return band;
    }

    /* Coming from a state that is not a voltage band — first call, a sensor
     * that just started working, a pack that just cooled down — there is no
     * hysteresis to apply. */
    if (previous != HK_POWER_LOW && previous != HK_POWER_CRITICAL &&
        previous != HK_POWER_SHUTDOWN && previous != HK_POWER_NORMAL) {
        return band;
    }

    /* Worse is immediate. A flat pack does not wait for a margin. */
    if (band > previous) {
        return band;
    }
    if (band == previous) {
        return band;
    }

    /* Better is earned: the reading has to clear the threshold it fell below
     * by the recovery margin. Without this a pack that sags while a note plays
     * and recovers between notes would flicker through the warning states. */
    if (inputs->pack_mv >= recover_above(previous, limits)) {
        return band;
    }
    return previous;
}

bool hk_power_audio_permitted(hk_power_state_t state, bool charging)
{
    /* ADR-0004: V1 does not play while charging. A product decision, not a
     * consequence of the voltage, so it is checked first and independently. */
    if (charging) {
        return false;
    }

    switch (state) {
    case HK_POWER_NORMAL:
    case HK_POWER_LOW:
        /* Low warns the user; it does not silence the music. Cutting audio at
         * the first warning would make the speaker useless well before the
         * pack is actually empty. */
        return true;
    case HK_POWER_UNKNOWN:
    case HK_POWER_CRITICAL:
    case HK_POWER_SHUTDOWN:
    case HK_POWER_SENSOR_FAULT:
    case HK_POWER_OVERHEAT:
        /* Including UNKNOWN and SENSOR_FAULT. A pack nobody can measure is not
         * a pack known to be fine, and driving an amplifier from one is how a
         * cell gets taken below its floor with nothing to notice. */
        return false;
    }
    return false;
}

const char *hk_power_state_name(hk_power_state_t state)
{
    switch (state) {
    case HK_POWER_UNKNOWN:      return "UNKNOWN";
    case HK_POWER_NORMAL:       return "NORMAL";
    case HK_POWER_LOW:          return "LOW";
    case HK_POWER_CRITICAL:     return "CRITICAL";
    case HK_POWER_SHUTDOWN:     return "SHUTDOWN";
    case HK_POWER_SENSOR_FAULT: return "SENSOR_FAULT";
    case HK_POWER_OVERHEAT:     return "OVERHEAT";
    }
    return "INVALID";
}
