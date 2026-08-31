/**
 * @file hk_ui.h
 * @brief Drives the function button and the RGB status LED.
 *
 * The thin hardware layer under two tested policy modules: hk_button decides
 * what a press means, hk_led decides what the LED should say, and this file
 * does nothing but read a pin, drive three PWM channels, and pass events on.
 *
 * It runs in its own low-priority task. The controls plan is explicit that LED
 * animation must never share a task with audio: a blocking or long-running UI
 * step there would show up as an I2S underrun.
 *
 * NOT YET VERIFIED ON HARDWARE. This compiles against ESP-IDF v5.5.1 and its
 * logic is covered by the host tests behind it, but no board has run it. The
 * PWM frequency in particular is a reasoned choice, not a measured one; see the
 * note on HK_UI_LED_PWM_HZ.
 */
#ifndef HK_UI_H
#define HK_UI_H

#include <stdbool.h>

#include "esp_err.h"
#include "hk_button.h"
#include "hk_led.h"

/**
 * LED PWM carrier frequency.
 *
 * Chosen above the audible band on purpose. The LED lines run through the same
 * enclosure as an analogue audio path and a Class-D amplifier, and a carrier
 * inside the audio band would be audible if it coupled at all. 25 kHz also
 * keeps 10-bit resolution available from the 80 MHz APB clock.
 *
 * This is reasoning, not measurement. The G3 noise measurement decides the
 * final value, and it may well move.
 */
#define HK_UI_LED_PWM_HZ 25000

/** How often the button is sampled. Well below the 50 ms debounce interval. */
#define HK_UI_POLL_MS 10

/** Called from the UI task when the button commits an action. */
typedef void (*hk_ui_event_cb_t)(hk_button_event_t event, void *context);

/**
 * Configure the pins and start the UI task.
 *
 * @param callback  invoked from the UI task on each committed button action;
 *                  keep it short and do not block in it
 * @param context   passed back to the callback
 */
esp_err_t hk_ui_start(hk_ui_event_cb_t callback, void *context);

/**
 * Tell the UI what the rest of the firmware is doing.
 *
 * The button hold level is filled in by the UI task itself and is ignored here.
 * Safe to call from any task.
 */
void hk_ui_set_status(const hk_led_inputs_t *status);

/**
 * Whether the button was already held when hk_ui_start() ran.
 *
 * Valid only after hk_ui_start() returns ESP_OK.
 */
bool hk_ui_recovery_requested(void);

#endif /* HK_UI_H */
