/**
 * @file hk_storage.h
 * @brief The two stores, and the wall between them.
 *
 * User settings live in the default `nvs` partition. Driver calibration lives
 * in `factory_cal`, a partition of its own. They are separate partitions rather
 * than two namespaces in one, because PRD-008 requires that a user reset cannot
 * reach the calibration, and a partition boundary is a guarantee where a naming
 * convention is a promise.
 *
 * Legacy keys. Until ADR-0023 the same `cal` namespace also carried three
 * per-device provisioning secrets written at manufacturing time: `prov_salt`
 * and `prov_verif` (the SRP6a pair behind protocomm Security 2) and `ap_pass`
 * (the setup network's WPA2 key). Setup no longer uses any of them -- both legs
 * run Security 1 with no proof of possession and the setup network is open --
 * and no code in this firmware reads them any more. A board flashed before that
 * decision still holds them; they are dead data, harmless, and need no reflash:
 * the partition is opened read-only here, so nothing will ever clear them
 * either. The names stay documented so a factory_cal dump from such a board
 * (write_profile.py --dump lists them) reads as history rather than as a
 * mystery.
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

/** Partition holding calibration (and, on pre-ADR-0023 boards, the legacy keys above). */
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

/** The calibration profile blob, written by the bench once G0/G2 have run. */
#define HK_STORAGE_PROFILE_KEY "profile"

/**
 * Whether a profile blob is present at all.
 *
 * Presence, not validity: this module cannot judge a profile without pulling in
 * the audio component, and the layering is worth more than the extra check.
 * hk_main judges the blob with hk_profile_load() -- the same function the
 * output backend builds its chain from -- at the build's output rate and
 * supply voltage, and reports the verdict here through
 * hk_storage_profile_judged(). Until it does, a present profile does not
 * permit audio.
 */
bool hk_storage_profile_present(void);

/**
 * Record the judge's verdict on the profile blob that is present.
 *
 * This module stores the verdict; it does not compute one. hk_main calls this
 * once, right after hk_storage_init(), with the answer hk_profile_load() gave
 * for the stored bytes at CONFIG_OUTPUT_SAMPLE_RATE_HZ and CONFIG_HK_SUPPLY_MV.
 * In a build without the receiver the judgement is hk_profile_from_blob(),
 * structural only, because that rate does not exist there; and when the store
 * is fail-safe or the blob cannot be read into one profile, false is reported
 * without a judge call at all. False is the value assumed until then, and
 * hk_storage_init() resets it to false: a verdict belongs to the bytes it was
 * reached on, and a store brought up again has to be judged again.
 */
void hk_storage_profile_judged(bool valid);

/** What was last reported through hk_storage_profile_judged(); false until then. */
bool hk_storage_profile_valid(void);

/**
 * Whether a trustworthy calibration profile is available.
 *
 * False means this device has never been calibrated, its profile is
 * unreadable, or the profile it carries was refused by the judge. The audio
 * path must stay in its safe state: no default profile is invented, because an
 * invented one would look exactly like a measured one while driving
 * unprotected drivers.
 *
 * Three things are required, and each was added because its absence was found
 * to open the gate. A matching schema version alone used to be treated as
 * enough: the provisioning credential generator writes a schema version into
 * this same namespace, so every provisioned device claimed to be calibrated --
 * on two real boards, unnoticed, because a second gate happened to be holding
 * the door. Presence of the profile itself was required from 2026-09-08. A
 * present blob that the judge refuses is the third case (ADR-0022): the DSP
 * backend refuses such a blob and writes digital zero, and a gate that read
 * presence alone would have released the DAC mute into that -- silence into
 * live amplifiers, the state the backend's own comment said could not occur.
 * So the verdict hk_main reports is required too.
 *
 * The bench exception (CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE) lifts the
 * ABSENCE refusal only. A present profile the judge refused stays refused on
 * the bench as well, for the reason above: the backend would then be running
 * with no chain, and an unmuted DAC in front of a chain that writes zeros is
 * not what the exception was written to allow.
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

/**
 * Read one user setting.
 *
 * @return true when a value was actually read.
 *
 * This used to take a fallback and return it on any failure, which folded
 * "nothing is stored", "the store is unusable" and "here is your value" into
 * one answer. hk_settings_resolve() needs those apart: a fresh device and a
 * device whose settings went bad both end up on the default, and only the
 * second one is worth a line in the log.
 */
bool hk_storage_user_read_u32(const char *key, uint32_t *out);

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
