#include "hk_pins.h"

#include <stddef.h>

const hk_pin_entry_t hk_pin_table[] = {
    {"i2s_bclk",  HK_PIN_I2S_BCLK},
    {"i2s_lrclk", HK_PIN_I2S_LRCLK},
    {"i2s_data",  HK_PIN_I2S_DATA},
    {"button",    HK_PIN_BUTTON},
    {"led_r",     HK_PIN_LED_R},
    {"led_g",     HK_PIN_LED_G},
    {"led_b",     HK_PIN_LED_B},
    {"dac_xsmt",  HK_PIN_DAC_XSMT},
    {NULL, -1},
};

int hk_pin_table_size(void)
{
    int count = 0;
    while (hk_pin_table[count].role != NULL) {
        count++;
    }
    return count;
}
