#include "hk_test.h"
#include "hk_led.h"

/*
 * The LED table in docs/controls-and-provisioning-plan.md, plus the priority
 * order. A single LED has to choose, and the choice is what these tests pin
 * down: which condition wins when several are true at once.
 */

static hk_led_state_t resolve(hk_led_inputs_t inputs)
{
    return hk_led_resolve(&inputs);
}

void test_led(void)
{
    /* --- one condition at a time --- */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.booting = true}), HK_LED_BOOT);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.provisioning = true}), HK_LED_PROVISIONING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.connecting = true}), HK_LED_CONNECTING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.ready = true}), HK_LED_READY);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true}), HK_LED_PLAYING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.ota = true}), HK_LED_OTA);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.battery_low = true}), HK_LED_BATTERY_LOW);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.error = true}), HK_LED_ERROR);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){0}), HK_LED_OFF);

    /* --- priority, which is the part that actually needs deciding --- */

    /* A fault beats everything, including an update in progress. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.error = true, .ota = true, .playing = true}),
                    HK_LED_ERROR);

    /* Cutting power mid-update is destructive, so the update warning beats
     * every merely informational state. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.ota = true, .playing = true, .battery_low = true}),
                    HK_LED_OTA);

    /* The user is holding the button and is about to commit to something
     * destructive: show them which, even while playing or on a low battery. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true, .battery_low = true,
                                              .button_hold = HK_BUTTON_HOLD_NETWORK_ARMED}),
                    HK_LED_HOLD_NETWORK);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true,
                                              .button_hold = HK_BUTTON_HOLD_FACTORY_ARMED}),
                    HK_LED_HOLD_FACTORY);

    /* But an update still outranks button feedback: the user can let go and
     * try again, whereas a half-written slot cannot be undone. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.ota = true,
                                              .button_hold = HK_BUTTON_HOLD_FACTORY_ARMED}),
                    HK_LED_OTA);

    /* Hold levels below the armed thresholds are not yet a warning. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true,
                                              .button_hold = HK_BUTTON_HOLD_SHORT}),
                    HK_LED_PLAYING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true,
                                              .button_hold = HK_BUTTON_HOLD_NEUTRAL}),
                    HK_LED_PLAYING);

    /* A low battery matters more than what is playing. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.battery_low = true, .playing = true, .ready = true}),
                    HK_LED_BATTERY_LOW);
    /* Activity beats mere readiness, and readiness beats still connecting. */
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.playing = true, .ready = true}), HK_LED_PLAYING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.ready = true, .connecting = true}), HK_LED_READY);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.connecting = true, .provisioning = true}),
                    HK_LED_CONNECTING);
    HK_CHECK_EQ_INT(resolve((hk_led_inputs_t){.provisioning = true, .booting = true}),
                    HK_LED_PROVISIONING);

    HK_CHECK_EQ_INT(hk_led_resolve(NULL), HK_LED_OFF);

    /* --- patterns --- */
    for (int state = HK_LED_OFF; state <= HK_LED_HOLD_FACTORY; state++) {
        const hk_led_pattern_t *pattern = hk_led_pattern((hk_led_state_t)state);
        HK_CHECK(pattern != NULL);
        HK_CHECK(pattern->brightness <= 100u);
        /* An animated pattern without a period would divide by zero in the
         * driver; a solid one with a period is a contradiction. */
        if (pattern->animation == HK_LED_ANIM_SOLID) {
            HK_CHECK_EQ_INT(pattern->period_ms, 0);
        } else {
            HK_CHECK(pattern->period_ms > 0);
        }
    }

    /* Out-of-range input must not read past the table. */
    HK_CHECK(hk_led_pattern((hk_led_state_t)999) == hk_led_pattern(HK_LED_OFF));

    /* Playing is the state that sits lit in a dark bedroom for hours, so it
     * must be the dimmest non-off state. */
    HK_CHECK(hk_led_pattern(HK_LED_PLAYING)->brightness <= HK_LED_BRIGHTNESS_AMBIENT);
    HK_CHECK(hk_led_pattern(HK_LED_PLAYING)->brightness
             < hk_led_pattern(HK_LED_READY)->brightness);

    /* The states the user must not miss are the brightest. */
    HK_CHECK_EQ_INT(hk_led_pattern(HK_LED_ERROR)->brightness, HK_LED_BRIGHTNESS_ALERT);
    HK_CHECK_EQ_INT(hk_led_pattern(HK_LED_OTA)->brightness, HK_LED_BRIGHTNESS_ALERT);

    /* Off is genuinely off. */
    const hk_led_pattern_t *off = hk_led_pattern(HK_LED_OFF);
    HK_CHECK_EQ_INT(off->red + off->green + off->blue, 0);
    HK_CHECK_EQ_INT(off->brightness, 0);

    HK_CHECK_EQ_STR(hk_led_state_name(HK_LED_HOLD_FACTORY), "hold_factory");
}
