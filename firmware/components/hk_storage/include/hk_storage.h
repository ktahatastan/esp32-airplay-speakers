/**
 * @file hk_storage.h
 * @brief The two stores, and the wall between them.
 *
 * User settings live in the default `nvs` partition. Driver calibration and the
 * per-device provisioning credentials live in `factory_cal`, a partition of
 * their own. They are separate partitions rather than two namespaces in one,
 * because PRD-008 requires that a user reset cannot reach the calibration, and
 * a partition boundary is a guarantee where a naming convention is a promise.
 *
 * This firmware opens factory_cal READ ONLY and never formats or erases it.
 * There is no calibration writer yet — that arrives with G2 — so nothing here
 * has any business modifying it. A build that cannot write a store cannot
 * corrupt it by accident.
 *
 * NOT YET VERIFIED ON HARDWARE.
 */
#ifndef HK_STORAGE_H
#define HK_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "hk_schema.h"

/** Partition holding calibration and provisioning credentials. */
#define HK_STORAGE_FACTORY_PARTITION "factory_cal"

/** Namespaces inside each partition. */
#define HK_STORAGE_USER_NAMESPACE    "user"
#define HK_STORAGE_FACTORY_NAMESPACE "cal"

/**
 * Open both stores and work out what state each is in.
 *
 * Never fails in a way that stops the device booting: an unusable store is
 * reported through hk_storage_factory_action() rather than by refusing to run.
 * A speaker that will not start cannot tell anyone why.
 */
esp_err_t hk_storage_init(void);

/** What was decided about the calibration store. */
hk_schema_action_t hk_storage_factory_action(void);

/** What was decided about the user settings store. */
hk_schema_action_t hk_storage_user_action(void);

/**
 * Whether a trustworthy calibration profile is available.
 *
 * False means this device has never been calibrated, or its profile is
 * unreadable. The audio path must stay in its safe state: no default profile
 * is invented, because an invented one would look exactly like a measured one
 * while driving unprotected drivers.
 */
bool hk_storage_audio_permitted(void);

/**
 * Restore user settings to defaults.
 *
 * Erases the user settings namespace only. It cannot reach factory_cal: that
 * partition is opened read-only and is never named by any erase call in this
 * module (PRD-008).
 */
esp_err_t hk_storage_user_reset(void);

/** Read a user setting. Returns the fallback when it is absent. */
uint32_t hk_storage_user_get_u32(const char *key, uint32_t fallback);

/** Write a user setting. */
esp_err_t hk_storage_user_set_u32(const char *key, uint32_t value);

/**
 * Read a blob from the calibration store.
 *
 * @param key     entry name
 * @param out     destination
 * @param length  in: capacity; out: bytes read
 */
esp_err_t hk_storage_factory_get_blob(const char *key, void *out, size_t *length);

#endif /* HK_STORAGE_H */
