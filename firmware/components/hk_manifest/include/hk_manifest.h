/**
 * @file hk_manifest.h
 * @brief Deciding whether a published release belongs on this device.
 *
 * Every check here runs on the manifest alone, before a single byte of the
 * image is fetched. That ordering is the point: a device that downloads first
 * and validates afterwards has already spent the airtime, the flash write
 * cycles and the battery, and on a speaker running from a 4S pack that is not
 * free.
 *
 * The rule throughout is that anything not understood is refused. An update
 * client that installs an image it could not fully check is how four speakers
 * become four bricks at the same moment, which is exactly the failure ADR-0008
 * builds two slots and a canary rollout to avoid.
 *
 * JSON parsing lives in the ESP-IDF layer. This module takes an already
 * populated struct plus a record of which fields were actually present, so the
 * decision — the part worth getting right — is pure C and tested on the host.
 */
#ifndef HK_MANIFEST_H
#define HK_MANIFEST_H

#include <stdbool.h>
#include <stdint.h>

#include "hk_version.h"

#define HK_MANIFEST_PRODUCT_MAX   32
#define HK_MANIFEST_CHANNEL_MAX   16
#define HK_MANIFEST_TARGET_MAX    16
#define HK_MANIFEST_HW_MAX        32
/*
 * Long enough for a real GitHub release URL with headroom. The obvious-looking
 * 96 was measured against this repository and came up two bytes short:
 *
 *   https://github.com/ktahatastan/esp32-airplay-speakers/releases/download/v0.1.0/harman-kardom.bin
 *
 * is 97 bytes, and a two-digit minor version makes it 98. A URL that does not
 * fit is refused as a missing field, so the failure would not have looked like
 * a length problem — it would have looked like every update being rejected,
 * forever, on all four speakers, from the first release onwards.
 */
#define HK_MANIFEST_ASSET_MAX     256
#define HK_MANIFEST_SHA256_CHARS  64

/** Fields the manifest may carry. Missing ones are recorded, not guessed. */
typedef enum {
    HK_MF_PRODUCT      = 1u << 0,
    HK_MF_VERSION      = 1u << 1,
    HK_MF_CHANNEL      = 1u << 2,
    HK_MF_TARGET       = 1u << 3,
    HK_MF_HW_REVISION  = 1u << 4,
    HK_MF_ASSET        = 1u << 5,
    HK_MF_SIZE         = 1u << 6,
    HK_MF_SHA256       = 1u << 7,
    HK_MF_SECURE_VER   = 1u << 8,
    HK_MF_MIN_UPDATER  = 1u << 9,
} hk_manifest_field_t;

/** Every field is required. An absent one is a manifest this build cannot judge. */
#define HK_MANIFEST_REQUIRED_FIELDS                                            \
    (HK_MF_PRODUCT | HK_MF_VERSION | HK_MF_CHANNEL | HK_MF_TARGET |            \
     HK_MF_HW_REVISION | HK_MF_ASSET | HK_MF_SIZE | HK_MF_SHA256 |             \
     HK_MF_SECURE_VER | HK_MF_MIN_UPDATER)

/** A parsed release manifest. */
typedef struct {
    uint32_t present;  /**< Bitmask of ::hk_manifest_field_t actually parsed */
    char     product[HK_MANIFEST_PRODUCT_MAX];
    char     version[HK_VERSION_MAX_TEXT];
    char     channel[HK_MANIFEST_CHANNEL_MAX];
    char     target[HK_MANIFEST_TARGET_MAX];
    char     hw_revision[HK_MANIFEST_HW_MAX];
    char     asset[HK_MANIFEST_ASSET_MAX];
    char     sha256[HK_MANIFEST_SHA256_CHARS + 1];
    char     min_updater_version[HK_VERSION_MAX_TEXT];
    uint32_t size;
    uint32_t secure_version;
} hk_manifest_t;

/** What this device is, and what it will accept. */
typedef struct {
    const char *product;
    const char *target;
    const char *hw_revision;
    const char *channel;         /**< The channel this device follows */
    const char *running_version;
    uint32_t    running_secure_version;
    uint32_t    slot_size;       /**< Bytes available in the inactive OTA slot */
} hk_device_t;

/** Why a manifest was refused. */
typedef enum {
    HK_MANIFEST_OK = 0,
    HK_MANIFEST_ERR_ARG,             /**< NULL argument */
    HK_MANIFEST_ERR_FIELD_MISSING,   /**< A required field was not in the manifest */
    HK_MANIFEST_ERR_PRODUCT,         /**< Built for a different product */
    HK_MANIFEST_ERR_TARGET,          /**< Built for a different chip */
    HK_MANIFEST_ERR_HW_REVISION,     /**< Built for different hardware */
    HK_MANIFEST_ERR_CHANNEL,         /**< Belongs to a channel this device does not follow */
    HK_MANIFEST_ERR_VERSION,         /**< Version is not strict SemVer */
    HK_MANIFEST_ERR_NOT_NEWER,       /**< Same as, or older than, what is running */
    HK_MANIFEST_ERR_UPDATER_TOO_OLD, /**< This build predates what the manifest requires */
    HK_MANIFEST_ERR_SHA256,          /**< Not 64 lowercase hex characters */
    HK_MANIFEST_ERR_SIZE,            /**< Zero, or larger than the slot */
    HK_MANIFEST_ERR_ROLLBACK,        /**< secure_version goes backwards */
} hk_manifest_err_t;

/**
 * Decide whether this release may be installed on this device.
 *
 * Checks run cheapest and most decisive first, and all of them before any
 * download begins.
 */
hk_manifest_err_t hk_manifest_validate(const hk_manifest_t *manifest,
                                       const hk_device_t *device);

/** Short name, for logs and tests. */
const char *hk_manifest_err_name(hk_manifest_err_t error);

#endif /* HK_MANIFEST_H */
