/**
 * @file hk_ota_client.h
 * @brief The ESP-IDF half of the update client: fetch, judge, write, or refuse.
 *
 * Everything worth arguing about lives in hk_manifest, hk_gate and hk_ota,
 * which are pure C and tested on the host. This file is the plumbing that
 * calls them in the right order and does the parts that need a chip: HTTPS,
 * JSON, and writing the inactive slot.
 *
 * The order is the design. The manifest is judged before a byte of image is
 * fetched, the gates are judged before the connection is opened, and the app
 * descriptor of the downloaded image is judged before the boot partition is
 * moved. Anything that fails aborts the transfer rather than finishing it.
 */
#ifndef HK_OTA_CLIENT_H
#define HK_OTA_CLIENT_H

#include "esp_err.h"
#include "hk_gate.h"
#include "hk_manifest.h"

/** Where to look, and what this device will accept. */
typedef struct {
    const char        *manifest_url;  /**< HTTPS URL of the release manifest */
    hk_device_t        device;
    hk_gate_inputs_t   gate_inputs;
    const hk_gate_limits_t *gate_limits;  /**< NULL means uncalibrated: blocks */
} hk_ota_request_t;

/**
 * Run one update attempt.
 *
 * @return ESP_OK when an image was written and armed for the next boot.
 *         ESP_ERR_NOT_FOUND when there is simply nothing newer.
 *         ESP_ERR_INVALID_STATE when a gate blocked the attempt.
 *         ESP_ERR_INVALID_VERSION when the manifest or the image was refused.
 *         Otherwise the underlying transport error.
 *
 * Never reboots. Deciding when a speaker restarts belongs to whatever knows
 * whether someone is listening to it.
 */
esp_err_t hk_ota_client_run(const hk_ota_request_t *request);

#endif /* HK_OTA_CLIENT_H */
