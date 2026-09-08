/**
 * @file hk_screen.h
 * @brief Which screen, and how one becomes another.
 *
 * The screen list is DERIVED, not commanded. hk_screen_choose() is a pure
 * function of the view snapshot, so there is no call anyone can forget to make
 * and no ordering anyone can get wrong; the reset paths in particular reach
 * their screens because the state resolves that way, not because something
 * remembered to say so.
 *
 * Every state in hk_led_state_t maps to a screen here. That is the property
 * worth keeping: the table is total, so "none of the above" does not exist and
 * the panel cannot arrive at a frame nobody designed.
 */
#ifndef HK_SCREEN_H
#define HK_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "hk_view.h"

typedef enum {
    HK_SCREEN_BOOT = 0,     /**< stars gather, the name is written */
    HK_SCREEN_PAIR_BLE,     /**< the provisioning QR, full width */
    HK_SCREEN_PAIR_AP,      /**< the Wi-Fi QR above the four-digit PIN */
    HK_SCREEN_CONNECTING,   /**< joining, with the signal meter filling */
    HK_SCREEN_READY,        /**< idle: name, signal, and the galaxy */
    HK_SCREEN_PLAYING,      /**< cover, marquee title, artist, progress ring */
    HK_SCREEN_CHARGING,     /**< pack percentage and the measured numbers */
    HK_SCREEN_BATTERY_LOW,
    HK_SCREEN_OTA,
    HK_SCREEN_HOLD_NETWORK, /**< the button is being held: network reset armed */
    HK_SCREEN_HOLD_FACTORY, /**< held further: factory reset armed */
    HK_SCREEN_AUDIO_LOCKED, /**< no calibration profile, so audio is refused */
    HK_SCREEN_ERROR,
    HK_SCREEN_COUNT
} hk_screen_id_t;

/** Which screen this snapshot means. Total over every state. */
hk_screen_id_t hk_screen_choose(const hk_view_t *view);

/** Short name, for logs. Never NULL. */
const char *hk_screen_name(hk_screen_id_t id);

/**
 * Draw one screen, completely, into `buf`.
 *
 * @param t_ms       monotonic ms, for animation
 * @param entered_ms when this screen became current, so an entry animation can
 *                   run from zero without the screen storing a phase
 * @param level      0-255 master brightness, used by the iris
 */
void hk_screen_render(uint16_t *buf, hk_screen_id_t id, const hk_view_t *view,
                      uint32_t t_ms, uint32_t entered_ms, uint8_t level);

/**
 * The volume overlay, composited over whatever is already in `buf`.
 *
 * An overlay rather than a screen: volume changes while something else is
 * happening, and replacing the screen would lose the thing the user was
 * looking at. `age_ms` drives its own entry, hold and dissolve.
 */
void hk_screen_volume_overlay(uint16_t *buf, const hk_view_t *view,
                              uint32_t age_ms);

/** How long the overlay stays up, entry and exit included. */
#define HK_SCREEN_VOLUME_MS 1600u

/** How long one screen takes to become another. */
#define HK_SCREEN_TRANSITION_MS 380u

#endif /* HK_SCREEN_H */
