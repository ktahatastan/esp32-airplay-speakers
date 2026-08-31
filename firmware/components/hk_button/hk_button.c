#include "hk_button.h"

/**
 * Elapsed milliseconds, wraparound-safe.
 *
 * Unsigned subtraction gives the correct interval across the 32-bit wrap, so
 * the button keeps working on a device that has been up for over 49 days.
 * Comparing timestamps directly would not.
 */
static uint32_t elapsed(uint32_t now, uint32_t since)
{
    return now - since;
}

void hk_button_init(hk_button_t *button, bool pressed_at_boot, uint32_t now_ms)
{
    button->stable_pressed = pressed_at_boot;
    button->raw_pressed = pressed_at_boot;
    button->raw_changed_ms = now_ms;
    button->pressed_since_ms = now_ms;
    button->recovery_request = pressed_at_boot;
    /* A button already down at boot asks for recovery; it must not also
     * produce a reset when the user lets go. */
    button->latched = pressed_at_boot;
}

hk_button_event_t hk_button_update(hk_button_t *button, bool raw_pressed, uint32_t now_ms)
{
    if (raw_pressed != button->raw_pressed) {
        button->raw_pressed = raw_pressed;
        button->raw_changed_ms = now_ms;
        return HK_BUTTON_EVENT_NONE;
    }

    if (elapsed(now_ms, button->raw_changed_ms) < HK_BUTTON_DEBOUNCE_MS) {
        return HK_BUTTON_EVENT_NONE;
    }
    if (raw_pressed == button->stable_pressed) {
        return HK_BUTTON_EVENT_NONE;
    }

    button->stable_pressed = raw_pressed;

    if (raw_pressed) {
        /* Press begins when the level first changed, not when the debounce
         * expired: otherwise every hold measures 50 ms short. */
        button->pressed_since_ms = button->raw_changed_ms;
        button->latched = false;
        return HK_BUTTON_EVENT_NONE;
    }

    if (button->latched) {
        return HK_BUTTON_EVENT_NONE;
    }
    button->latched = true;

    uint32_t held = elapsed(button->raw_changed_ms, button->pressed_since_ms);
    if (held >= HK_BUTTON_FACTORY_MS) {
        return HK_BUTTON_EVENT_FACTORY_RESET;
    }
    if (held >= HK_BUTTON_NETWORK_MS) {
        return HK_BUTTON_EVENT_NETWORK_RESET;
    }
    if (held >= HK_BUTTON_SHORT_MIN_MS && held <= HK_BUTTON_SHORT_MAX_MS) {
        return HK_BUTTON_EVENT_SHORT_PRESS;
    }
    /* Either shorter than an intentional tap, or in the dead window between
     * the tap and the network reset. Both do nothing on purpose. */
    return HK_BUTTON_EVENT_NONE;
}

hk_button_hold_t hk_button_hold(const hk_button_t *button, uint32_t now_ms)
{
    if (!button->stable_pressed || button->latched) {
        return HK_BUTTON_HOLD_NONE;
    }
    uint32_t held = elapsed(now_ms, button->pressed_since_ms);
    if (held >= HK_BUTTON_FACTORY_MS) {
        return HK_BUTTON_HOLD_FACTORY_ARMED;
    }
    if (held >= HK_BUTTON_NETWORK_MS) {
        return HK_BUTTON_HOLD_NETWORK_ARMED;
    }
    if (held > HK_BUTTON_SHORT_MAX_MS) {
        return HK_BUTTON_HOLD_NEUTRAL;
    }
    return HK_BUTTON_HOLD_SHORT;
}

bool hk_button_recovery_requested(const hk_button_t *button)
{
    return button->recovery_request;
}

void hk_button_clear_recovery(hk_button_t *button)
{
    button->recovery_request = false;
}
