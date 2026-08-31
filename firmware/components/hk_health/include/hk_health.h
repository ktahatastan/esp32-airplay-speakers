/**
 * @file hk_health.h
 * @brief Deciding whether a freshly installed image has earned its place.
 *
 * A new image boots in ESP-IDF's "pending verify" state. If the application
 * calls esp_ota_mark_app_valid_cancel_rollback() it becomes permanent; if it
 * does not, the bootloader reverts to the previous slot on the next restart.
 * There is no third option, and both mistakes are expensive in opposite
 * directions:
 *
 *   Confirming too eagerly keeps a broken image. The speaker is then stuck on
 *   firmware that does not work, and the way out is a USB cable.
 *
 *   Never confirming reverts a perfectly good image on the next restart, which
 *   looks like "updates simply do not stick" and is maddening to diagnose,
 *   because nothing anywhere reports an error.
 *
 * So this module returns three verdicts rather than a boolean, and the third
 * one — keep waiting — is the common case for the first half-minute. Deciding
 * early is not a virtue here.
 *
 * The rule that shapes everything else: a subsystem that has not reported yet
 * is UNKNOWN, not failed. Subsystems come up at their own pace, and treating
 * silence as failure would roll back every image that merely booted slowly.
 * Silence only becomes failure at the deadline, when a subsystem that should
 * have spoken by now has not.
 *
 * Pure C, like hk_gate, and for the same reason: the decision is the part
 * worth getting right, and it can be exercised exhaustively on a laptop years
 * before it ever runs during a real first boot.
 */
#ifndef HK_HEALTH_H
#define HK_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

/** How one first-boot criterion is doing. */
typedef enum {
    HK_HEALTH_UNKNOWN = 0, /**< Has not reported yet. Not a failure. */
    HK_HEALTH_PASS,
    HK_HEALTH_FAIL,        /**< Reported, and bad. */
    HK_HEALTH_SKIP,        /**< Not applicable to this build, deliberately */
} hk_health_state_t;

/**
 * The five first-boot criteria from docs/03-firmware/ota-and-release-plan.md.
 *
 * @c network deserves a word, because getting it wrong is how a good image
 * gets rolled back. It is PASS when the device joined its network *or* when it
 * deliberately opened provisioning — a speaker whose owner changed their Wi-Fi
 * password has a network problem, not a firmware problem, and reverting to the
 * previous image would not help because that image cannot connect either. Only
 * report FAIL here when the network stack itself failed to start.
 */
typedef struct {
    hk_health_state_t storage;   /**< NVS opened, schema version understood, migration done */
    hk_health_state_t network;   /**< Joined, or provisioning open on purpose */
    hk_health_state_t audio;     /**< I2S/DAC task running and feeding its watchdog */
    hk_health_state_t telemetry; /**< Pack voltage and NTC read, and plausible */
    uint32_t uptime_ms;          /**< Milliseconds since boot */
    bool     critical_fault;     /**< A panic, abort or watchdog reset was recorded */
} hk_health_inputs_t;

/**
 * Timings. Both come from the plan rather than from a measurement, so unlike
 * hk_gate's limits they have defaults worth stating — but they are still
 * passed in, so a test can drive the boundaries without waiting half a minute.
 */
typedef struct {
    uint32_t settle_ms;   /**< Minimum clean run before confirming (plan: 30 s) */
    uint32_t deadline_ms; /**< After this, a silent subsystem counts as failed */
} hk_health_limits_t;

/** Plan defaults. deadline is comfortably past settle; see hk_health_evaluate. */
#define HK_HEALTH_SETTLE_MS_DEFAULT   30000u
#define HK_HEALTH_DEADLINE_MS_DEFAULT 120000u

/** What the caller should do about it. */
typedef enum {
    HK_HEALTH_WAIT = 0, /**< Not yet. Ask again shortly. */
    HK_HEALTH_CONFIRM,  /**< esp_ota_mark_app_valid_cancel_rollback() */
    HK_HEALTH_ROLLBACK, /**< esp_ota_mark_app_invalid_rollback_and_reboot() */
} hk_health_verdict_t;

/** Why, for the log and for tests. */
typedef enum {
    HK_HEALTH_REASON_OK = 0,
    HK_HEALTH_REASON_ARG,              /**< NULL argument */
    HK_HEALTH_REASON_BAD_LIMITS,       /**< deadline not after settle */
    HK_HEALTH_REASON_UNSKIPPABLE,      /**< A criterion every build has was declared SKIP */
    HK_HEALTH_REASON_CRITICAL_FAULT,
    HK_HEALTH_REASON_STORAGE_FAILED,
    HK_HEALTH_REASON_NETWORK_FAILED,
    HK_HEALTH_REASON_AUDIO_FAILED,
    HK_HEALTH_REASON_TELEMETRY_FAILED,
    HK_HEALTH_REASON_STORAGE_SILENT,   /**< Still UNKNOWN at the deadline */
    HK_HEALTH_REASON_NETWORK_SILENT,
    HK_HEALTH_REASON_AUDIO_SILENT,
    HK_HEALTH_REASON_TELEMETRY_SILENT,
    HK_HEALTH_REASON_SETTLING,         /**< All good so far, still inside settle_ms */
    HK_HEALTH_REASON_INCOMPLETE,       /**< Something has not reported, deadline not reached */
} hk_health_reason_t;

/**
 * Decide what to do about a pending-verify image.
 *
 * Checked in order of decisiveness: a recorded fault, then an explicit
 * failure, then silence past the deadline, then whether it has run cleanly for
 * long enough. Anything else is WAIT.
 *
 * @param inputs current state; NULL yields WAIT, never CONFIRM
 * @param limits timings; NULL, or a deadline not strictly after settle, yields
 *               WAIT — a build that cannot judge itself must not confirm itself
 *
 * @note @c storage and @c network may not be SKIP. They belong to components
 *       that are in every build, so declaring them inapplicable is not a
 *       description of the build — it is switching the check off while leaving
 *       something that still looks like a check. SKIP exists for @c audio and
 *       @c telemetry, whose components do not exist yet.
 * @param reason optional; may be NULL
 *
 * @note @c uptime_ms is measured from boot and this runs only in the first
 *       minutes, so the 32-bit millisecond wrap that hk_button has to handle
 *       cannot be reached here.
 */
hk_health_verdict_t hk_health_evaluate(const hk_health_inputs_t *inputs,
                                       const hk_health_limits_t *limits,
                                       hk_health_reason_t *reason);

/** Short names, for logs and tests. */
const char *hk_health_verdict_name(hk_health_verdict_t verdict);
const char *hk_health_reason_name(hk_health_reason_t reason);

#endif /* HK_HEALTH_H */
