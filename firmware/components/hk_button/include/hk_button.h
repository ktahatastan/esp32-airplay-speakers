/**
 * @file hk_button.h
 * @brief Function button state machine.
 *
 * Implements the button behaviour in docs/controls-and-provisioning-plan.md
 * and requirement PRD-005. Pure C with time passed in, so every threshold and
 * every refusal can be tested on the host without a board.
 *
 * The rules that matter are the ones about NOT acting:
 *
 *   - An action is decided on RELEASE, never on crossing a threshold. Holding
 *     past 12 s and continuing to hold erases nothing; the user can keep
 *     holding, and only letting go commits.
 *   - A short tap must never delete stored Wi-Fi credentials. The window
 *     between the short press and the network reset is deliberately dead: a
 *     release there does nothing at all, so a hesitant press cannot land on a
 *     destructive action by accident.
 *   - A factory reset restores user settings only. It must not touch the
 *     factory calibration partition (PRD-008); that is enforced by the caller,
 *     and this module never emits an event that implies otherwise.
 */
#ifndef HK_BUTTON_H
#define HK_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

/** A level change must persist this long before it is believed. */
#define HK_BUTTON_DEBOUNCE_MS 50u

/** Shortest press that counts as intentional; below this it is noise. */
#define HK_BUTTON_SHORT_MIN_MS 100u

/** Longest press still treated as a short tap. */
#define HK_BUTTON_SHORT_MAX_MS 1500u

/** Hold at least this long, then release, to clear Wi-Fi credentials. */
#define HK_BUTTON_NETWORK_MS 5000u

/** Hold at least this long, then release, to restore user settings. */
#define HK_BUTTON_FACTORY_MS 12000u

/** What the user asked for, emitted once, on release. */
typedef enum {
    HK_BUTTON_EVENT_NONE = 0,
    HK_BUTTON_EVENT_SHORT_PRESS,    /**< Open a provisioning window */
    HK_BUTTON_EVENT_NETWORK_RESET,  /**< Clear Wi-Fi credentials only */
    HK_BUTTON_EVENT_FACTORY_RESET,  /**< Restore user settings; calibration survives */
} hk_button_event_t;

/**
 * How long the button has been held, as a level.
 *
 * This drives immediate LED feedback so the user can see which action they are
 * about to commit to, and can back out by holding on to the next level.
 */
typedef enum {
    HK_BUTTON_HOLD_NONE = 0,        /**< Not pressed */
    HK_BUTTON_HOLD_SHORT,           /**< Inside the short-press window */
    HK_BUTTON_HOLD_NEUTRAL,         /**< Past short, before network: releasing does nothing */
    HK_BUTTON_HOLD_NETWORK_ARMED,   /**< Releasing now clears Wi-Fi credentials */
    HK_BUTTON_HOLD_FACTORY_ARMED,   /**< Releasing now restores user settings */
} hk_button_hold_t;

/** Opaque-ish state. Zero it and call hk_button_init(). */
typedef struct {
    bool     stable_pressed;    /**< Debounced level */
    bool     raw_pressed;       /**< Last raw level seen */
    uint32_t raw_changed_ms;    /**< When the raw level last changed */
    uint32_t pressed_since_ms;  /**< When the debounced press began */
    bool     recovery_request;  /**< Button was already held at boot */
    bool     latched;           /**< Event already emitted for this press */
} hk_button_t;

/**
 * Initialise.
 *
 * @param button          state to initialise
 * @param pressed_at_boot raw level at startup; true requests recovery provisioning
 * @param now_ms          current millisecond tick
 */
void hk_button_init(hk_button_t *button, bool pressed_at_boot, uint32_t now_ms);

/**
 * Feed one sample.
 *
 * Call at a steady rate, faster than the debounce interval. Time is a plain
 * millisecond counter; wraparound is handled by unsigned arithmetic, so a
 * device running for more than 49 days keeps working.
 *
 * @return the event decided by this sample, or HK_BUTTON_EVENT_NONE
 */
hk_button_event_t hk_button_update(hk_button_t *button, bool raw_pressed, uint32_t now_ms);

/** Current hold level, for LED feedback. */
hk_button_hold_t hk_button_hold(const hk_button_t *button, uint32_t now_ms);

/** True when the button was held at boot, asking for recovery provisioning. */
bool hk_button_recovery_requested(const hk_button_t *button);

/** Clear the recovery request once the caller has acted on it. */
void hk_button_clear_recovery(hk_button_t *button);

#endif /* HK_BUTTON_H */
