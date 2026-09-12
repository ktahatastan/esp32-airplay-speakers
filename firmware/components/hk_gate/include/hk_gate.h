/**
 * @file hk_gate.h
 * @brief When an update may start.
 *
 * Implements the gate table in docs/03-firmware/ota-and-release-plan.md. A
 * manifest that passes hk_manifest_validate says the release BELONGS on this
 * device; this says the device is in a fit state to take it right now.
 *
 * The defining rule is that an unknown state blocks: a caller that has no
 * inputs to offer gets a refusal, not a default, because a device that cannot
 * say what it is doing is not a device that should start rewriting its flash.
 *
 * Interrupting an update is not catastrophic on its own: the image is written
 * to the inactive slot and a power loss simply aborts it. What these gates
 * actually protect is the user's evening. An update that starts mid-song is a
 * worse outcome than waiting until tonight.
 */
#ifndef HK_GATE_H
#define HK_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** What the device knows about itself right now. */
typedef struct {
    bool audio_active;       /**< AirPlay is playing, or the buffer is not empty */
    bool wifi_connected;
    bool update_in_progress;
} hk_gate_inputs_t;

/** Why an update may not start. */
typedef enum {
    HK_GATE_GO = 0,
    HK_GATE_NO_INPUTS,            /**< The caller offered no state: nothing to judge */
    HK_GATE_UPDATE_IN_PROGRESS,
    HK_GATE_NO_WIFI,
    HK_GATE_AUDIO_ACTIVE,
} hk_gate_result_t;

/**
 * Decide whether an update may start now.
 *
 * @param inputs current state; NULL blocks with HK_GATE_NO_INPUTS
 */
hk_gate_result_t hk_gate_evaluate(const hk_gate_inputs_t *inputs);

/**
 * Whether a blocked result is worth retrying soon.
 *
 * Playback ends and routers come back, so those are worth another look
 * shortly. A caller that offered no inputs will offer none in five minutes
 * either, and retrying only burns power.
 */
bool hk_gate_retry_soon(hk_gate_result_t result);

/** Short name, for logs and tests. */
const char *hk_gate_result_name(hk_gate_result_t result);

#endif /* HK_GATE_H */
