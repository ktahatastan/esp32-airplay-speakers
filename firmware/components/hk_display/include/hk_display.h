/**
 * @file hk_display.h
 * @brief The round panel, driven from the state the LED already resolves.
 *
 * ADR-0017. The screen has NO STATE OF ITS OWN, and that is the design rather
 * than a simplification: a surface driven by a sequence of commands is one that
 * can be left showing the previous thing when a transition is missed, and the
 * likeliest place for that is right after a network reset. This renders from
 * hk_led_resolve() -- the same function, the same inputs -- so the screen and
 * the LED cannot disagree, and there is no screen state to get stuck in.
 *
 * Nothing here has run on a panel. The wiring is ADR-0017's; if it is wrong the
 * boot report says which step failed rather than leaving a dark screen and no
 * explanation.
 */
#ifndef HK_DISPLAY_H
#define HK_DISPLAY_H

#include <stdbool.h>

#include "esp_err.h"

#include "hk_led.h"

/** Longest setup PIN the panel will show. WPA2 caps a passphrase at 63. */
#define HK_DISPLAY_PIN_MAX 63

/**
 * Bring up SPI, the panel and the render task.
 *
 * Never fatal to the caller: a speaker with a dead screen is still a speaker.
 * Returns the underlying error so the boot report can name it, and the rest of
 * the device carries on either way.
 */
esp_err_t hk_display_start(void);

/** Whether the panel came up. False means every set_state call is a no-op. */
bool hk_display_present(void);

#endif /* HK_DISPLAY_H */
