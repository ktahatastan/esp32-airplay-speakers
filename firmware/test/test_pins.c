#include "hk_test.h"
#include "hk_pins.h"

/*
 * The compiler already rejects a duplicate or forbidden assignment through the
 * static assertions in hk_pins.h. These tests cover what a static assertion
 * cannot: that the table still matches the pin table published in
 * docs/02-hardware/circuit-and-wiring-plan.md section 3.1, function by function.
 * If the document changes, this test is the thing that should fail first.
 */
void test_pins(void)
{
    HK_CHECK_EQ_INT(HK_PIN_I2S_BCLK, 4);
    HK_CHECK_EQ_INT(HK_PIN_I2S_LRCLK, 5);
    HK_CHECK_EQ_INT(HK_PIN_I2S_DATA, 6);
    HK_CHECK_EQ_INT(HK_PIN_BUTTON, 7);
    HK_CHECK_EQ_INT(HK_PIN_LED_R, 8);
    HK_CHECK_EQ_INT(HK_PIN_LED_G, 9);
    HK_CHECK_EQ_INT(HK_PIN_LED_B, 10);
    HK_CHECK_EQ_INT(HK_PIN_I2C_SDA, 11);
    HK_CHECK_EQ_INT(HK_PIN_I2C_SCL, 12);
    HK_CHECK_EQ_INT(HK_PIN_AMP_MUTE, 21);
    HK_CHECK_EQ_INT(HK_PIN_DAC_XSMT, 13);
    HK_CHECK_EQ_INT(HK_PIN_BATT_SENSE, 1);
    HK_CHECK_EQ_INT(HK_PIN_NTC_SENSE, 2);

    HK_CHECK_EQ_INT(hk_pin_table_size(), HK_PIN_COUNT);

    /* No two roles may share a GPIO, and none may land on a strapping or
     * native-USB pin. Checked here as well as at compile time so the intent is
     * visible in the test report. */
    const int forbidden[] = {
        0, 3, 45, 46,                       /* strapping */
        19, 20,                             /* native USB, needed for recovery */
        26, 27, 28, 29, 30, 31, 32,         /* SPI flash, and PSRAM CS1 on 26 */
        33, 34, 35, 36, 37,                 /* octal PSRAM DQ4-DQ7 and DQS (the R8) */
        43, 44,                             /* UART0: ROM boot log and console */
        22, 23, 24, 25,                     /* do not exist on the S3 die */
    };
    int size = hk_pin_table_size();
    for (int i = 0; i < size; i++) {
        int gpio = hk_pin_table[i].gpio;
        HK_CHECK(gpio >= 0 && gpio <= HK_GPIO_MAX);
        for (unsigned f = 0; f < sizeof(forbidden) / sizeof(forbidden[0]); f++) {
            HK_CHECK(gpio != forbidden[f]);
        }
        for (int j = i + 1; j < size; j++) {
            HK_CHECK(gpio != hk_pin_table[j].gpio);
        }
    }

    /* --- pins that must never carry a mute line ---
     * The silicon drives GPIO18/19/20 HIGH during power-up, and GPIO0/39/43/44
     * come up with weak internal pull-ups. An active-low mute on any of them
     * would release the amplifier before any software exists, into drivers
     * whose impedance is the open G0 blocker. Pinned as a test because the
     * static asserts cover collisions, not reset levels — 18, 39 and 47 are
     * all perfectly legal assignments that would simply be wrong here. */
    {
        const int unsafe_for_mute[] = {0, 18, 19, 20, 39, 43, 44};
        for (unsigned u = 0; u < sizeof(unsafe_for_mute) / sizeof(unsafe_for_mute[0]); u++) {
            HK_CHECK(HK_PIN_AMP_MUTE != unsafe_for_mute[u]);
            HK_CHECK(HK_PIN_DAC_XSMT != unsafe_for_mute[u]);
        }
    }

    /* --- the last two ADC1 channels ---
     * ADC1 is GPIO1-10 on this part and GPIO4-10 are already spent, so these
     * two are the only Wi-Fi-safe analogue inputs left. If either ever moves
     * outside ADC1, battery and temperature sensing lose their controller. */
    HK_CHECK(HK_PIN_BATT_SENSE >= 1 && HK_PIN_BATT_SENSE <= 10);
    HK_CHECK(HK_PIN_NTC_SENSE >= 1 && HK_PIN_NTC_SENSE <= 10);

    /* The DAC runs in 3-wire mode with SCK grounded, so no GPIO may be spent on
     * a master clock. Guard against someone "helpfully" adding one. */
#ifdef HK_PIN_I2S_MCLK
    HK_CHECK(0 && "MCLK must not be assigned: PCM5102A runs 3-wire with SCK to GND");
#endif
}
