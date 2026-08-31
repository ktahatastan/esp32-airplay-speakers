#include "hk_manifest.h"

#include <stddef.h>
#include <string.h>

/** Exact, bounded string equality. A prefix is not a match. */
static bool equals(const char *stored, const char *expected, size_t capacity)
{
    if (expected == NULL) {
        return false;
    }
    size_t expected_len = strlen(expected);
    if (expected_len >= capacity) {
        return false;
    }
    return strncmp(stored, expected, capacity) == 0 && strlen(stored) == expected_len;
}

/** A SHA-256 digest is exactly 64 lowercase hex characters. */
static bool is_sha256(const char *text)
{
    size_t length = strnlen(text, HK_MANIFEST_SHA256_CHARS + 1);
    if (length != HK_MANIFEST_SHA256_CHARS) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char c = text[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex) {
            /* Uppercase is rejected rather than folded: the digest is compared
             * against one this device computes, and accepting two spellings of
             * the same value invites a comparison that succeeds by accident. */
            return false;
        }
    }
    return true;
}

hk_manifest_err_t hk_manifest_validate(const hk_manifest_t *manifest,
                                       const hk_device_t *device)
{
    if (manifest == NULL || device == NULL) {
        return HK_MANIFEST_ERR_ARG;
    }

    /* Anything absent means this build cannot judge the release. Refusing is
     * the only safe reading of a manifest written by tooling we do not know. */
    if ((manifest->present & HK_MANIFEST_REQUIRED_FIELDS) != HK_MANIFEST_REQUIRED_FIELDS) {
        return HK_MANIFEST_ERR_FIELD_MISSING;
    }

    /* Identity first: cheap, and it rules out the whole release outright. */
    if (!equals(manifest->product, device->product, sizeof(manifest->product))) {
        return HK_MANIFEST_ERR_PRODUCT;
    }
    if (!equals(manifest->target, device->target, sizeof(manifest->target))) {
        return HK_MANIFEST_ERR_TARGET;
    }
    if (!equals(manifest->hw_revision, device->hw_revision, sizeof(manifest->hw_revision))) {
        return HK_MANIFEST_ERR_HW_REVISION;
    }
    /* A device following stable must not take a canary build. ADR-0008 promotes
     * the same signed asset after canary validation, so a stable device only
     * ever sees a manifest whose channel already says stable. */
    if (!equals(manifest->channel, device->channel, sizeof(manifest->channel))) {
        return HK_MANIFEST_ERR_CHANNEL;
    }

    hk_version_t offered;
    if (hk_version_parse(manifest->version, &offered) != HK_VERSION_OK) {
        /* Strict SemVer only, so a prerelease tag lands here rather than being
         * silently installed on a device that follows stable. */
        return HK_MANIFEST_ERR_VERSION;
    }

    /* Does this build understand the manifest at all? A future release may add
     * a field whose absence changes what a check means, so the manifest names
     * the oldest firmware allowed to act on it. */
    hk_version_t min_updater;
    hk_version_t running;
    if (hk_version_parse(manifest->min_updater_version, &min_updater) != HK_VERSION_OK ||
        hk_version_parse(device->running_version, &running) != HK_VERSION_OK) {
        return HK_MANIFEST_ERR_VERSION;
    }
    if (hk_version_compare(&running, &min_updater) < 0) {
        return HK_MANIFEST_ERR_UPDATER_TOO_OLD;
    }

    if (!hk_version_should_update(manifest->version, device->running_version)) {
        return HK_MANIFEST_ERR_NOT_NEWER;
    }

    if (!is_sha256(manifest->sha256)) {
        return HK_MANIFEST_ERR_SHA256;
    }

    if (manifest->size == 0u || manifest->size > device->slot_size) {
        /* Discovering this mid-download would mean a half-written slot. */
        return HK_MANIFEST_ERR_SIZE;
    }

    /* Anti-rollback. A release may raise the secure version; it may never lower
     * it, because installing an image with a known flaw is the thing a secure
     * version exists to prevent. */
    if (manifest->secure_version < device->running_secure_version) {
        return HK_MANIFEST_ERR_ROLLBACK;
    }

    return HK_MANIFEST_OK;
}

const char *hk_manifest_err_name(hk_manifest_err_t error)
{
    switch (error) {
    case HK_MANIFEST_OK:               return "ok";
    case HK_MANIFEST_ERR_ARG:          return "bad_argument";
    case HK_MANIFEST_ERR_FIELD_MISSING:return "field_missing";
    case HK_MANIFEST_ERR_PRODUCT:      return "wrong_product";
    case HK_MANIFEST_ERR_TARGET:       return "wrong_target";
    case HK_MANIFEST_ERR_HW_REVISION:  return "wrong_hardware";
    case HK_MANIFEST_ERR_CHANNEL:      return "wrong_channel";
    case HK_MANIFEST_ERR_VERSION:      return "bad_version";
    case HK_MANIFEST_ERR_NOT_NEWER:    return "not_newer";
    case HK_MANIFEST_ERR_UPDATER_TOO_OLD: return "updater_too_old";
    case HK_MANIFEST_ERR_SHA256:       return "bad_sha256";
    case HK_MANIFEST_ERR_SIZE:         return "bad_size";
    case HK_MANIFEST_ERR_ROLLBACK:     return "secure_version_rollback";
    }
    return "unknown";
}
