#include "hk_test.h"
#include "hk_button.h"

/*
 * PRD-005 and docs/controls-and-provisioning-plan.md.
 *
 * Most of these tests are about the button NOT doing something. A function
 * button that erases Wi-Fi on a stray touch, or wipes settings the moment a
 * threshold is crossed, is worse than no button at all.
 */

/**
 * Hold the button for exactly `duration` milliseconds, then release.
 *
 * The release lands on start + duration precisely, because the thresholds are
 * inclusive boundaries and an approximate helper would silently test a
 * different number than the one written in the call.
 */
static hk_button_event_t hold_and_release(hk_button_t *button, uint32_t *clock, uint32_t duration)
{
    uint32_t start = *clock;
    hk_button_event_t event = HK_BUTTON_EVENT_NONE;

    for (uint32_t t = start; t - start < duration; t += 10u) {
        *clock = t;
        /* A press alone never commits anything; only release does. */
        HK_CHECK_EQ_INT(hk_button_update(button, true, t), HK_BUTTON_EVENT_NONE);
    }

    uint32_t release = start + duration;
    for (uint32_t t = release; t - release <= HK_BUTTON_DEBOUNCE_MS + 40u; t += 10u) {
        *clock = t;
        hk_button_event_t e = hk_button_update(button, false, t);
        if (e != HK_BUTTON_EVENT_NONE) {
            event = e;
        }
    }
    return event;
}

void test_button(void)
{
    hk_button_t button;
    uint32_t clock = 1000u;

    /* --- the three documented gestures --- */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 300u), HK_BUTTON_EVENT_SHORT_PRESS);

    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 6000u), HK_BUTTON_EVENT_NETWORK_RESET);

    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 13000u), HK_BUTTON_EVENT_FACTORY_RESET);

    /* --- refusals --- */

    /* Contact noise shorter than the debounce interval is not a press. */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 20u), HK_BUTTON_EVENT_NONE);

    /* A brush against the panel is not an intentional tap. */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 80u), HK_BUTTON_EVENT_NONE);

    /* The dead window: a hesitant press that lands here must do nothing at
     * all, rather than fall through to the nearest destructive action. */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 3000u), HK_BUTTON_EVENT_NONE);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 4900u), HK_BUTTON_EVENT_NONE);

    /* --- boundaries, from the documented table --- */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_SHORT_MIN_MS),
                    HK_BUTTON_EVENT_SHORT_PRESS);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_SHORT_MAX_MS),
                    HK_BUTTON_EVENT_SHORT_PRESS);
    /* One millisecond past each inclusive boundary falls to the next band. */
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_SHORT_MAX_MS + 1u),
                    HK_BUTTON_EVENT_NONE);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_SHORT_MIN_MS - 1u),
                    HK_BUTTON_EVENT_NONE);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_NETWORK_MS - 1u),
                    HK_BUTTON_EVENT_NONE);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_FACTORY_MS - 1u),
                    HK_BUTTON_EVENT_NETWORK_RESET);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_NETWORK_MS),
                    HK_BUTTON_EVENT_NETWORK_RESET);
    hk_button_init(&button, false, clock);
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, HK_BUTTON_FACTORY_MS),
                    HK_BUTTON_EVENT_FACTORY_RESET);

    /* --- nothing happens until release --- */
    hk_button_init(&button, false, clock);
    uint32_t start = clock;
    for (uint32_t t = start; t - start <= 20000u; t += 100u) {
        HK_CHECK_EQ_INT(hk_button_update(&button, true, t), HK_BUTTON_EVENT_NONE);
    }
    /* Held for twenty seconds, well past every threshold, and still nothing
     * has been erased. Only letting go commits. */
    HK_CHECK_EQ_INT(hk_button_hold(&button, start + 20000u), HK_BUTTON_HOLD_FACTORY_ARMED);

    /* --- hold levels drive the LED feedback --- */
    hk_button_init(&button, false, clock);
    start = clock;
    hk_button_update(&button, true, start);
    hk_button_update(&button, true, start + HK_BUTTON_DEBOUNCE_MS + 10u);
    HK_CHECK_EQ_INT(hk_button_hold(&button, start + 200u), HK_BUTTON_HOLD_SHORT);
    HK_CHECK_EQ_INT(hk_button_hold(&button, start + 3000u), HK_BUTTON_HOLD_NEUTRAL);
    HK_CHECK_EQ_INT(hk_button_hold(&button, start + 6000u), HK_BUTTON_HOLD_NETWORK_ARMED);
    HK_CHECK_EQ_INT(hk_button_hold(&button, start + 13000u), HK_BUTTON_HOLD_FACTORY_ARMED);

    /* --- held at boot asks for recovery, and does not also reset --- */
    hk_button_init(&button, true, clock);
    HK_CHECK(hk_button_recovery_requested(&button));
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 13000u), HK_BUTTON_EVENT_NONE);
    hk_button_clear_recovery(&button);
    HK_CHECK(!hk_button_recovery_requested(&button));

    /* --- one press yields at most one event --- */
    hk_button_init(&button, false, clock);
    start = clock;
    for (uint32_t t = start; t - start <= 6000u; t += 10u) {
        hk_button_update(&button, true, t);
    }
    uint32_t release = start + 6010u;
    int events = 0;
    for (uint32_t t = release; t - release <= 500u; t += 10u) {
        if (hk_button_update(&button, false, t) != HK_BUTTON_EVENT_NONE) {
            events++;
        }
    }
    HK_CHECK_EQ_INT(events, 1);

    /* --- contact bounce during a hold must not commit anything --- */
    /* This is what the debounce is actually for. Without it, a 20 ms glitch
     * six seconds into a factory-reset hold reads as a release and erases the
     * user's Wi-Fi credentials on a gesture they never made. */
    hk_button_init(&button, false, 0u);
    for (uint32_t t = 0u; t < 6000u; t += 10u) {
        HK_CHECK_EQ_INT(hk_button_update(&button, true, t), HK_BUTTON_EVENT_NONE);
    }
    /* Glitch: the contact opens for 20 ms, well under the debounce interval. */
    HK_CHECK_EQ_INT(hk_button_update(&button, false, 6000u), HK_BUTTON_EVENT_NONE);
    HK_CHECK_EQ_INT(hk_button_update(&button, false, 6010u), HK_BUTTON_EVENT_NONE);
    HK_CHECK_EQ_INT(hk_button_update(&button, true, 6020u), HK_BUTTON_EVENT_NONE);
    for (uint32_t t = 6030u; t < 13000u; t += 10u) {
        HK_CHECK_EQ_INT(hk_button_update(&button, true, t), HK_BUTTON_EVENT_NONE);
    }
    /* The hold was never interrupted, so the real release still measures the
     * full 13 s and commits the action the user actually asked for. */
    hk_button_update(&button, false, 13000u);
    HK_CHECK_EQ_INT(hk_button_update(&button, false, 13000u + HK_BUTTON_DEBOUNCE_MS + 10u),
                    HK_BUTTON_EVENT_FACTORY_RESET);

    /* A glitch inside a short press must not split it into two presses. */
    hk_button_init(&button, false, 0u);
    hk_button_update(&button, true, 0u);
    hk_button_update(&button, true, 100u);
    HK_CHECK_EQ_INT(hk_button_update(&button, false, 200u), HK_BUTTON_EVENT_NONE);
    HK_CHECK_EQ_INT(hk_button_update(&button, true, 220u), HK_BUTTON_EVENT_NONE);
    hk_button_update(&button, true, 400u);
    hk_button_update(&button, false, 500u);
    HK_CHECK_EQ_INT(hk_button_update(&button, false, 500u + HK_BUTTON_DEBOUNCE_MS + 10u),
                    HK_BUTTON_EVENT_SHORT_PRESS);

    /* --- a button held through boot shows no warning it will not honour --- */
    /* The LED reads the hold level. If a latched boot-hold still reported
     * FACTORY_ARMED, the LED would blink the factory-reset warning while
     * releasing the button does nothing at all. */
    hk_button_init(&button, true, 0u);
    HK_CHECK_EQ_INT(hk_button_hold(&button, 13000u), HK_BUTTON_HOLD_NONE);
    HK_CHECK_EQ_INT(hk_button_hold(&button, 6000u), HK_BUTTON_HOLD_NONE);

    /* --- the millisecond counter wraps every 49 days --- */
    /* A device left on that long must still work, so the machine compares
     * intervals with unsigned arithmetic rather than comparing timestamps. */
    uint32_t near_wrap = 0xFFFFF000u;
    hk_button_init(&button, false, near_wrap);
    clock = near_wrap;
    HK_CHECK_EQ_INT(hold_and_release(&button, &clock, 6000u), HK_BUTTON_EVENT_NETWORK_RESET);
    HK_CHECK(clock < near_wrap);  /* the clock really did wrap during the press */
}
