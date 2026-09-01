/*
 * Turning a manifest's JSON into the struct hk_manifest_validate() judges.
 *
 * This used to live inside hk_ota_client.c, next to the HTTPS code, which made
 * it unreachable from a host test: the file cannot compile without ESP-IDF. So
 * the two halves of the OTA contract were verified separately and never
 * against each other — the generator's field NAMES were compared with the
 * client's, but no test ever fed a generated manifest's VALUES to the
 * validator that has to accept them.
 *
 * cJSON is plain C99 with no ESP dependencies, so pulling the parsing out here
 * costs nothing on the device and makes the whole path testable: real
 * generator output, real parser, real validator.
 */
#include "hk_ota.h"

#include <stddef.h>
#include <string.h>

#include "cJSON.h"

/** Copy a JSON string field, recording that it was present and fitted. */
static bool take_string(const cJSON *root, const char *key, char *out, size_t out_size,
                        uint32_t bit, uint32_t *present)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    const size_t len = strlen(item->valuestring);
    if (len == 0u || len >= out_size) {
        /* Too long is refused rather than truncated. A truncated product name
         * or sha256 would go on to be compared, and might even match. */
        return false;
    }
    memcpy(out, item->valuestring, len + 1u);
    *present |= bit;
    return true;
}

/** Copy a JSON number field that must be a non-negative integer. */
static bool take_u32(const cJSON *root, const char *key, uint32_t *out,
                     uint32_t bit, uint32_t *present)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    if (item->valuedouble < 0.0 || item->valuedouble > 4294967295.0) {
        return false;
    }
    /* Reject a fractional size or version outright; rounding one would invent
     * a number the publisher did not write. */
    if ((double)(uint32_t)item->valuedouble != item->valuedouble) {
        return false;
    }
    *out = (uint32_t)item->valuedouble;
    *present |= bit;
    return true;
}

bool hk_manifest_parse(const char *json, hk_manifest_t *out)
{
    /* This was a static helper called from exactly one place, so it never
     * checked its arguments. As public API it is called from a test harness
     * and, one day, from something else. */
    if (json == NULL || out == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    uint32_t present = 0;

    (void)take_string(root, "product", out->product, sizeof(out->product),
                      HK_MF_PRODUCT, &present);
    (void)take_string(root, "version", out->version, sizeof(out->version),
                      HK_MF_VERSION, &present);
    (void)take_string(root, "channel", out->channel, sizeof(out->channel),
                      HK_MF_CHANNEL, &present);
    (void)take_string(root, "target", out->target, sizeof(out->target),
                      HK_MF_TARGET, &present);
    (void)take_string(root, "hw_revision", out->hw_revision, sizeof(out->hw_revision),
                      HK_MF_HW_REVISION, &present);
    (void)take_string(root, "asset", out->asset, sizeof(out->asset),
                      HK_MF_ASSET, &present);
    (void)take_string(root, "sha256", out->sha256, sizeof(out->sha256),
                      HK_MF_SHA256, &present);
    (void)take_string(root, "min_updater_version", out->min_updater_version,
                      sizeof(out->min_updater_version), HK_MF_MIN_UPDATER, &present);
    (void)take_u32(root, "size", &out->size, HK_MF_SIZE, &present);
    (void)take_u32(root, "secure_version", &out->secure_version, HK_MF_SECURE_VER, &present);

    out->present = present;
    cJSON_Delete(root);
    return true;
}

