/**
 * @file hk_ota.h
 * @brief Checking the image that actually arrived against the manifest that promised it.
 *
 * hk_manifest decides whether a release BELONGS on this device, using nothing
 * but the manifest. This module runs afterwards, on the bytes that turned up,
 * and asks a different question: is this the image the manifest described?
 *
 * That question needs asking because ESP-IDF does not ask it. In v5.5.1
 * esp_https_ota_get_img_desc() validates exactly one thing about the
 * descriptor it returns — that magic_word equals ESP_APP_DESC_MAGIC_WORD
 * (esp_https_ota.c:589-594). Nothing in components/app_update,
 * components/esp_https_ota or components/bootloader_support ever reads
 * project_name; a grep for it across all three returns no hits. So a correctly
 * built ESP32-S3 image from an entirely different project passes every check
 * IDF performs, and would be written to the inactive slot and booted.
 *
 * The manifest is a promise made over HTTPS. The descriptor is what the flash
 * will actually run. Comparing them costs three string compares and turns a
 * swapped, stale or mis-uploaded asset into a refused update instead of a
 * speaker running someone else's firmware.
 *
 * Pure C on purpose: the caller reads the descriptor through ESP-IDF and hands
 * the fields over, so the decision is testable on the host with no chip.
 */
#ifndef HK_OTA_H
#define HK_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hk_manifest.h"

/** ESP-IDF's esp_app_desc_t caps project_name and version at 32 bytes each. */
#define HK_OTA_PROJECT_MAX 32

/**
 * The parts of esp_app_desc_t worth judging, copied out of the image header.
 *
 * @c valid is false when the descriptor could not be read or failed IDF's own
 * magic-word check. It is a separate field rather than a NULL pointer so that
 * "no descriptor" is a state the caller has to pass explicitly and cannot
 * reach by forgetting to set something.
 */
typedef struct {
    bool     valid;
    char     project_name[HK_OTA_PROJECT_MAX + 1];
    char     version[HK_VERSION_MAX_TEXT];
    uint32_t secure_version;
} hk_ota_image_t;

/** Why a downloaded image was refused. */
typedef enum {
    HK_OTA_IMAGE_OK = 0,
    HK_OTA_IMAGE_ERR_ARG,            /**< NULL argument */
    HK_OTA_IMAGE_ERR_NO_DESC,        /**< Descriptor missing or failed the magic-word check */
    HK_OTA_IMAGE_ERR_PROJECT,        /**< project_name is not this product */
    HK_OTA_IMAGE_ERR_VERSION,        /**< Image version disagrees with the manifest */
    HK_OTA_IMAGE_ERR_SECURE_VERSION, /**< secure_version disagrees with the manifest */
} hk_ota_image_err_t;

/**
 * Check a downloaded image against the manifest that advertised it.
 *
 * Every field must agree exactly. A mismatch is never treated as a newer or
 * more interesting build: the two sources disagreeing means at least one of
 * them is wrong, and there is no way from here to tell which.
 *
 * The caller is expected to have run hk_manifest_validate() already; this does
 * not repeat those checks. It compares project_name against the manifest's
 * product, which hk_manifest_validate has by then confirmed equals the
 * device's own product string.
 */
hk_ota_image_err_t hk_ota_image_check(const hk_ota_image_t *image,
                                      const hk_manifest_t *manifest);

/** Short name, for logs and tests. */
const char *hk_ota_image_err_name(hk_ota_image_err_t error);

/**
 * Whether a manifest's asset URL is one this device will fetch.
 *
 * Requires https, a host ending in an expected GitHub domain, no embedded
 * credentials and no control characters.
 *
 * This is a sanity check on the manifest, not the security boundary. ESP-IDF
 * follows redirects, and GitHub relies on one: a release download starts at
 * github.com and lands on release-assets.githubusercontent.com. What actually
 * protects the transfer is TLS against the certificate bundle, and what
 * protects the flash is hk_ota_image_check() plus the manifest's sha256. This
 * only stops a manifest from pointing somewhere obviously wrong — a plain-http
 * URL, or a host nobody involved in this project controls.
 */
bool hk_ota_asset_url_ok(const char *url);

/**
 * Parse a release manifest into the struct hk_manifest_validate() judges.
 *
 * Deliberately separate from the HTTPS client so that the whole chain —
 * generator output, parser, validator — can be exercised on a host with no
 * chip. Missing or unusable fields are simply left out of @c out->present
 * rather than guessed at, so the validator sees exactly which ones arrived.
 *
 * @return false only when @p json is not valid JSON at all. A manifest that
 *         parses but is missing fields returns true; judging that is
 *         hk_manifest_validate()'s job, not this one's.
 */
bool hk_manifest_parse(const char *json, hk_manifest_t *out);

/* ===================== release channels ===================== */

/** Channel values as stored in user settings. */
typedef enum {
    HK_OTA_CHANNEL_STABLE = 0,
    HK_OTA_CHANNEL_CANARY = 1,
} hk_ota_channel_t;

/**
 * How many consecutive rollbacks before this device stops updating itself.
 *
 * A speaker that rolls back, downloads the same release again, rolls back
 * again and repeats is not being cautious — it is spending its battery and its
 * flash write cycles on the same mistake, nightly. Three attempts is enough to
 * distinguish a transient failure from a release this device cannot run.
 */
#define HK_OTA_MAX_ROLLBACKS 3u

/**
 * The channel name a manifest must carry for this device to accept it.
 *
 * Returns the stable channel for any value it does not recognise. A corrupted
 * setting should leave a speaker on the conservative channel rather than
 * silently promote it to candidate builds.
 */
const char *hk_ota_channel_name(uint32_t channel);

/**
 * Whether this device should still be looking for updates.
 *
 * False once it has rolled back too many times in a row. Recovering needs a
 * person: either a release that fixes whatever is wrong, installed over USB,
 * or the counter cleared deliberately. That is the right cost — the
 * alternative is a device that keeps trying forever and never says so.
 */
bool hk_ota_updates_allowed(uint32_t consecutive_rollbacks);

/** Longest manifest address this builder can produce, including the NUL. */
#define HK_OTA_URL_MAX 160u

/**
 * Build the manifest address for a channel.
 *
 * Each channel has its own fixed address rather than sharing one, because
 * GitHub's `releases/latest` resolves to the newest NON-prerelease and so can
 * only ever serve stable. A canary device pointed at it would fetch a manifest
 * for a channel it does not follow and correctly refuse it — safe, and useless.
 *
 * The addresses point at two pointer releases whose tags never change, so the
 * device's address never changes either; publishing moves what those tags
 * carry, not where the device looks.
 *
 * @return false if @p out is NULL or too small, leaving nothing written. A
 *         truncated URL would be a device quietly checking the wrong place.
 */
bool hk_ota_manifest_url(char *out, size_t size, uint32_t channel);

#endif /* HK_OTA_H */
