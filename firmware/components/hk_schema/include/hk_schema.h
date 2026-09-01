/**
 * @file hk_schema.h
 * @brief What to do when stored data does not match the firmware that found it.
 *
 * The project keeps two stores, and they are not equivalent:
 *
 *   user_settings   what the owner chose. Recoverable: if it is lost they set
 *                   it again in a minute.
 *   factory_cal     the driver protection profile, the crossover and limiter
 *                   limits, and the per-device provisioning credentials.
 *                   NOT recoverable: it comes from measurements taken once,
 *                   with equipment, against these specific drivers.
 *
 * That asymmetry drives every rule below. Discarding user settings is a small
 * annoyance; discarding calibration means the tweeter loses the protection that
 * G2 established, and no amount of rebooting brings it back.
 *
 * Pure C. Deciding this correctly matters more than any single line of NVS
 * code, so it is separated out and tested exhaustively on the host.
 */
#ifndef HK_SCHEMA_H
#define HK_SCHEMA_H

#include <stdbool.h>
#include <stdint.h>

/** Schema version this firmware writes. Bump when a stored layout changes. */
#define HK_SCHEMA_USER_VERSION    1u
#define HK_SCHEMA_FACTORY_VERSION 1u

/** Which store is being opened. */
typedef enum {
    HK_STORE_USER = 0,   /**< user_settings: recoverable */
    HK_STORE_FACTORY,    /**< factory_cal: not recoverable */
} hk_store_t;

/** What was found in the store. */
typedef enum {
    HK_SCHEMA_FOUND_ABSENT = 0, /**< Nothing there: a fresh device or a wiped partition */
    HK_SCHEMA_FOUND_MATCH,      /**< Same version this firmware writes */
    HK_SCHEMA_FOUND_OLDER,      /**< Written by older firmware */
    HK_SCHEMA_FOUND_NEWER,      /**< Written by NEWER firmware, e.g. after an OTA rollback */
    HK_SCHEMA_FOUND_CORRUPT,    /**< Unreadable */
} hk_schema_found_t;

/** What the caller should do about it. */
typedef enum {
    HK_SCHEMA_USE = 0,       /**< Read and write normally */
    HK_SCHEMA_MIGRATE,       /**< Convert forward, then use */
    HK_SCHEMA_WRITE_DEFAULTS,/**< Initialise this store from defaults */
    HK_SCHEMA_READ_ONLY,     /**< Read what is understood; write nothing */
    HK_SCHEMA_FAIL_SAFE,     /**< Do not use this store at all; run conservatively */
} hk_schema_action_t;

/**
 * Classify what was read against what this firmware expects.
 *
 * @param present true when a version value was read at all
 * @param found   the version read; ignored when present is false
 * @param current the version this firmware writes
 */
hk_schema_found_t hk_schema_classify(bool present, uint32_t found, uint16_t current);

/**
 * Decide what to do.
 *
 * The two stores differ where it counts:
 *
 *   NEWER   user settings are reset to defaults, because a rollback should
 *           leave a usable speaker. Calibration goes read-only instead: newer
 *           firmware may have written a profile this build cannot express, and
 *           overwriting it would destroy measurements that cost bench time.
 *   CORRUPT user settings are reset. Calibration is NOT erased, because an
 *           unreadable profile may still be recoverable by hand, and because
 *           erasing it silently would leave a speaker driving unprotected
 *           tweeters with defaults nobody measured.
 *   ABSENT  user settings get defaults. Missing calibration is fail-safe: this
 *           device has never been calibrated, and inventing a profile is
 *           exactly what the project forbids.
 */
hk_schema_action_t hk_schema_resolve(hk_store_t store, hk_schema_found_t found);

/**
 * Whether this build can convert a store written at @p from_version.
 *
 * hk_schema_resolve() answers HK_SCHEMA_MIGRATE for anything older, which is
 * the right decision and only half an answer: deciding to convert is not the
 * same as having a converter. Today there is exactly one schema version and
 * therefore no conversions, so this returns false for everything — and that is
 * the point. The moment someone bumps HK_SCHEMA_USER_VERSION to 2, every
 * existing device resolves to MIGRATE, and without this check the old store
 * would be read as though it were the new layout. Silently, on all four.
 */
bool hk_schema_can_migrate(hk_store_t store, uint32_t from_version);

/**
 * What to do when a conversion was called for and none exists.
 *
 * The two stores degrade differently, for the same reasons they always do:
 * user settings fall back to defaults, because losing a volume setting is a
 * minute of the owner's time; calibration goes fail-safe, because a profile
 * this build cannot convert is a profile it must not pretend to understand.
 */
hk_schema_action_t hk_schema_without_migration(hk_store_t store);

/**
 * Classify, resolve, and check that any conversion asked for actually exists.
 *
 * This is what callers should use. hk_schema_resolve() can return MIGRATE on
 * its own, and a caller that treats that as "carry on" gets a store read
 * through the wrong layout. This one cannot return MIGRATE unless a converter
 * is genuinely present, so the dangerous case has to be reached deliberately.
 */
hk_schema_action_t hk_schema_plan(hk_store_t store, bool present, uint32_t found_version);

/**
 * hk_schema_plan() with the current version supplied rather than compiled in.
 *
 * This exists so the migration path can be reached at all. With one schema
 * version there is no valid older one — version 0 classifies as corrupt, not
 * as older — so hk_schema_plan() cannot produce MIGRATE today and the guard
 * against acting on it would sit untested until the first version bump, which
 * is exactly when it has to work.
 */
hk_schema_action_t hk_schema_plan_with(hk_store_t store, bool present,
                                       uint32_t found_version, uint16_t current);

/**
 * Whether an action allows driving audio at normal levels.
 *
 * False means the driver protection profile is unavailable or untrusted, and
 * the caller must stay in its safe state rather than fall back to a default
 * nobody measured.
 */
bool hk_schema_audio_permitted(hk_schema_action_t factory_action);

/** Short name, for logs and tests. */
const char *hk_schema_action_name(hk_schema_action_t action);
const char *hk_schema_found_name(hk_schema_found_t found);

#endif /* HK_SCHEMA_H */
