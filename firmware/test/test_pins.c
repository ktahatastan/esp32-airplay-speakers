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

    HK_CHECK_EQ_INT(hk_pin_table_size(), HK_PIN_COUNT);

    /* No two roles may share a GPIO, and none may land on a strapping or
     * native-USB pin. Checked here as well as at compile time so the intent is
     * visible in the test report. */
    const int forbidden[] = {0, 3, 19, 20, 45, 46};
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

    /* The DAC runs in 3-wire mode with SCK grounded, so no GPIO may be spent on
     * a master clock. Guard against someone "helpfully" adding one. */
#ifdef HK_PIN_I2S_MCLK
    HK_CHECK(0 && "MCLK must not be assigned: PCM5102A runs 3-wire with SCK to GND");
#endif
}
