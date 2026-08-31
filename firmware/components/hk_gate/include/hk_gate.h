/**
 * @file hk_gate.h
 * @brief When an update may start.
 *
 * Implements the gate table in docs/03-firmware/ota-and-release-plan.md. A
 * manifest that passes hk_manifest_validate says the release BELONGS on this
 * device; this says the device is in a fit state to take it right now.
 *
 * The defining rule is that an unknown reading blocks. The plan is explicit
 * that a battery threshold has to come from the G3/G4 measurements and that
 * without them an update does not start, so there is no default threshold
 * anywhere in this module: the limits are passed in, and they come from the
 * calibration store. A device with no trustworthy calibration therefore cannot
 * update, which is the same conclusion hk_storage reaches about audio and for
 * the same reason — a number nobody measured looks exactly like a measured one.
 *
 * Interrupting an update is not catastrophic on its own: the image is written
 * to the inactive slot and a power loss simply aborts it. What these gates
 * actually protect is the user's evening. An update that starts mid-song, or
 * that flattens a pack the owner was about to carry into the garden, is a worse
 * outcome than waiting until tonight.
 */
#ifndef HK_GATE_H
#define HK_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** Sentinel for a reading the device does not have. */
#define HK_GATE_BATTERY_UNKNOWN 0u
#define HK_GATE_TEMPERATURE_UNKNOWN INT16_MIN

/** What the device knows about itself right now. */
typedef struct {
    bool     audio_active;      /**< AirPlay is playing, or the buffer is not empty */
    bool     wifi_connected;
    bool     update_in_progress;
    bool     charging;          /**< External power present */
    uint32_t battery_mv;        /**< Pack voltage; HK_GATE_BATTERY_UNKNOWN if unread */
    int16_t  temperature_c;     /**< Cell temperature; HK_GATE_TEMPERATURE_UNKNOWN if unread */
} hk_gate_inputs_t;

/**
 * Thresholds, from the calibration store.
 *
 * There is deliberately no default. Every value here has to come from a
 * measurement that has not been taken yet, and inventing one would produce a
 * device that looks calibrated.
 */
typedef struct {
    uint32_t min_battery_mv;    /**< Below this, do not start (G3/G4) */
    int16_t  max_temperature_c; /**< Above this, do not start (G4) */
} hk_gate_limits_t;

/** Why an update may not start. */
typedef enum {
    HK_GATE_GO = 0,
    HK_GATE_NO_LIMITS,            /**< No calibrated thresholds: nothing to judge against */
    HK_GATE_UPDATE_IN_PROGRESS,
    HK_GATE_NO_WIFI,
    HK_GATE_AUDIO_ACTIVE,
    HK_GATE_BATTERY_UNREAD,       /**< Pack voltage unknown */
    HK_GATE_BATTERY_LOW,
    HK_GATE_TEMPERATURE_UNREAD,   /**< Cell temperature unknown */
    HK_GATE_TEMPERATURE_HIGH,
} hk_gate_result_t;

/**
 * Decide whether an update may start now.
 *
 * @param inputs current state; NULL blocks
 * @param limits calibrated thresholds; NULL blocks with HK_GATE_NO_LIMITS
 */
hk_gate_result_t hk_gate_evaluate(const hk_gate_inputs_t *inputs,
                                  const hk_gate_limits_t *limits);

/**
 * Whether a blocked result is worth retrying soon.
 *
 * Playback ends and batteries charge, so those are worth another look shortly.
 * A device with no calibration will still have none in five minutes, and
 * retrying only burns power.
 */
bool hk_gate_retry_soon(hk_gate_result_t result);

/** Short name, for logs and tests. */
const char *hk_gate_result_name(hk_gate_result_t result);

#endif /* HK_GATE_H */
