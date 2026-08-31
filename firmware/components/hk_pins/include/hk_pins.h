/**
 * @file hk_pins.h
 * @brief The single source of truth for Harman Kardom GPIO assignment.
 *
 * These numbers mirror the candidate pin table in
 * docs/02-hardware/circuit-and-wiring-plan.md section 3.1 and the generated
 * schematics. If the two ever disagree, the document and the drawing win and
 * this header is wrong.
 *
 * Status: the BOARD is accepted (ESP32-S3 N16R8, ADR-0010); the PIN ASSIGNMENT
 * is still a candidate. It cannot be marked accepted until the purchased
 * board's own schematic and a boot test confirm it. Nothing here should be
 * treated as verified silicon behaviour.
 *
 * The constraints below are enforced by the compiler rather than by review,
 * because a strapping-pin collision produces a board that boots into the wrong
 * mode instead of an obvious error.
 *
 * Plain integers are used instead of gpio_num_t so this header stays usable
 * from host-side unit tests that never link against ESP-IDF.
 */
#ifndef HK_PINS_H
#define HK_PINS_H

#include <stdint.h>

/** Largest GPIO number available on the ESP32-S3. */
#define HK_GPIO_MAX 48

/* --- I2S to the PCM5102A -------------------------------------------------
 * SCK/MCLK is deliberately absent: the module runs in 3-wire mode with SCK
 * tied to ground and the bit clock driving the internal PLL. Assigning a GPIO
 * to it here would contradict the schematic.
 */
#define HK_PIN_I2S_BCLK   4  /**< PCM5102A BCK  */
#define HK_PIN_I2S_LRCLK  5  /**< PCM5102A LCK/LRCK */
#define HK_PIN_I2S_DATA   6  /**< PCM5102A DIN, one direction only */

/* --- User interface ------------------------------------------------------ */
#define HK_PIN_BUTTON     7  /**< Active low to ground, 10 k pull-up candidate */
#define HK_PIN_LED_R      8  /**< PWM, series resistor on the board */
#define HK_PIN_LED_G      9  /**< PWM */
#define HK_PIN_LED_B     10  /**< PWM */

/* --- Optional telemetry --------------------------------------------------- */
#define HK_PIN_I2C_SDA   11  /**< INA226, optional */
#define HK_PIN_I2C_SCL   12  /**< INA226, optional */

/* --- Mute lines ----------------------------------------------------------
 * Both are active low, and both are held in their SAFE state by an external
 * pull-down, not by the ESP32. That is not belt-and-braces, it is the whole
 * mechanism: every candidate GPIO on this part leaves reset high-impedance
 * with its output driver disabled, and stays that way through the ROM, the
 * second-stage bootloader and app init — hundreds of milliseconds during which
 * the amplifier would be free to reproduce whatever is on its input. A 10 k
 * pull-down against the part's 45 k typical internal pull dominates it better
 * than four to one.
 *
 * So the firmware's job is to RELEASE mute, never to create it. If this
 * firmware never runs at all, the speakers stay quiet.
 *
 * GPIO18, GPIO19 and GPIO20 are excluded from mute duty by name: the silicon
 * drives them HIGH during power-up. So are GPIO0, GPIO39, GPIO43 and GPIO44,
 * which come up with weak internal pull-ups. Any of them on an active-low mute
 * would release the amplifier before software exists, into drivers whose
 * impedance is still the open G0 blocker.
 *
 * HK_PIN_AMP_MUTE is a RESERVATION. Whether the XH-A232 board exposes an
 * accessible SD pad is still an open item in the wiring plan, so this may end
 * up connected to nothing. Reserving it costs a pin that nothing else wanted;
 * discovering the need after the harness is soldered costs the harness.
 */
#define HK_PIN_AMP_MUTE  21  /**< TPA3110 SD, active low. External pull-down. */
#define HK_PIN_DAC_XSMT  13  /**< PCM5102A XSMT, active low. External pull-down. */

/* --- Analogue sense ------------------------------------------------------
 * ADC1 on the ESP32-S3 is exactly GPIO1-10, and this design already spends
 * GPIO4-10 on I2S, the button and the LED. GPIO3 is ADC1_CH2 but is a
 * strapping pin. That leaves GPIO1 and GPIO2 as the last two ADC1 channels on
 * the part, and the design needs exactly two analogue measurements.
 *
 * They are claimed here rather than left free precisely because they are the
 * last two: a future signal taking one would be invisible until someone tried
 * to add battery sensing and found nowhere to put it. Moving an analogue
 * channel after assembly means moving a divider, not a wire.
 *
 * ADC2 (GPIO11-20) is not an equivalent fallback on this part: only ADC1
 * supports the continuous/DMA controller, per SOC_ADC_DIG_SUPPORTED_UNIT.
 *
 * Both are RESERVATIONS. The divider ratio, the NTC network and every
 * threshold come from G3/G4 measurements that have not been taken; nothing
 * here implies a calibrated reading exists.
 */
#define HK_PIN_BATT_SENSE 1  /**< ADC1_CH0, 4S pack through a divider */
#define HK_PIN_NTC_SENSE  2  /**< ADC1_CH1, cell thermistor */

/**
 * Number of GPIOs this design claims:
 * 3 I2S + 1 button + 3 RGB + 2 I2C + 2 mute + 2 analogue.
 */
#define HK_PIN_COUNT 13

/** Every assigned pin, as a bit mask. */
#define HK_PIN_MASK ( \
      (1ULL << HK_PIN_I2S_BCLK)  | (1ULL << HK_PIN_I2S_LRCLK) | \
      (1ULL << HK_PIN_I2S_DATA)  | (1ULL << HK_PIN_BUTTON)    | \
      (1ULL << HK_PIN_LED_R)     | (1ULL << HK_PIN_LED_G)     | \
      (1ULL << HK_PIN_LED_B)     | (1ULL << HK_PIN_I2C_SDA)   | \
      (1ULL << HK_PIN_I2C_SCL)   | (1ULL << HK_PIN_AMP_MUTE)  | \
      (1ULL << HK_PIN_DAC_XSMT)  | (1ULL << HK_PIN_BATT_SENSE)| \
      (1ULL << HK_PIN_NTC_SENSE))

/**
 * GPIO numbers that do not exist on the ESP32-S3 die.
 *
 * soc_caps.h states it plainly: "0~48 valid except 22~25". A define naming one
 * of these compiles and then fails at runtime in a way that looks like a
 * wiring fault.
 */
#define HK_GPIO_NONEXISTENT_MASK \
      ((1ULL << 22) | (1ULL << 23) | (1ULL << 24) | (1ULL << 25))

/**
 * Pins this design must never claim.
 *
 * This mask is the ONLY thing standing between a typo and a board that will
 * not boot, which is worth stating because it is easy to assume ESP-IDF would
 * catch it. On this exact configuration it does not:
 *
 *   esp_mspi_pin_reserve() (spi_flash/flash_ops.c:160-177) skips the DQS and
 *   D4-D7 entries whenever the FLASH is quad. An N16R8 is quad flash with
 *   OCTAL PSRAM, so GPIO33-37 — the five pins the PSRAM is actively driving —
 *   are never reserved at all. And reservation would not save anything even
 *   where it happens: gpio_config() never consults the reserved mask, and
 *   LEDC merely logs a warning before wiring the signal anyway.
 *
 * So the entries below are not a tidy restatement of the datasheet. They are
 * the guard.
 *
 * - GPIO0, 3, 45, 46: strapping. Driving one at reset selects a boot mode,
 *   the flash voltage or the ROM log setting.
 * - GPIO19, 20: native USB D-/D+. The USB/UART recovery path documented in
 *   the OTA plan has to stay usable in every release, and it is also the
 *   remedy of last resort for a bad key or a bad image.
 * - GPIO26-32: SPI flash, plus PSRAM CS1 on 26.
 * - GPIO33-37: octal PSRAM DQ4-DQ7 and DQS. The R8 in N16R8. On a quad-PSRAM
 *   part these five would be free, which is exactly the trap: the same module
 *   name with a different suffix has a different answer.
 * - GPIO43, 44: UART0, the ROM boot log and this build's console. Losing them
 *   means losing the first thing anyone reads when a board misbehaves.
 */
#define HK_PIN_FORBIDDEN_MASK ( \
      (1ULL << 0)  | (1ULL << 3)  | (1ULL << 45) | (1ULL << 46) | \
      (1ULL << 19) | (1ULL << 20) | \
      (1ULL << 26) | (1ULL << 27) | (1ULL << 28) | (1ULL << 29) | \
      (1ULL << 30) | (1ULL << 31) | (1ULL << 32) | (1ULL << 33) | \
      (1ULL << 34) | (1ULL << 35) | (1ULL << 36) | (1ULL << 37) | \
      (1ULL << 43) | (1ULL << 44))

_Static_assert(__builtin_popcountll(HK_PIN_MASK) == HK_PIN_COUNT,
               "hk_pins: two functions share a GPIO, or HK_PIN_COUNT is stale");
_Static_assert((HK_PIN_MASK & HK_PIN_FORBIDDEN_MASK) == 0,
               "hk_pins: an assignment lands on a reserved pin -- strapping, native USB, "
               "SPI flash, octal PSRAM or UART0. See HK_PIN_FORBIDDEN_MASK");
_Static_assert((HK_PIN_MASK >> (HK_GPIO_MAX + 1)) == 0,
               "hk_pins: an assignment exceeds the highest ESP32-S3 GPIO");
_Static_assert((HK_PIN_MASK & HK_GPIO_NONEXISTENT_MASK) == 0,
               "hk_pins: an assignment names a GPIO the ESP32-S3 does not have");

/** Human-readable role of a pin, for diagnostics and tests. */
typedef struct {
    const char *role;
    int         gpio;
} hk_pin_entry_t;

/** The assignment as data, so a test can iterate it. Terminated by role == NULL. */
extern const hk_pin_entry_t hk_pin_table[];

/** Number of entries in ::hk_pin_table, excluding the terminator. */
int hk_pin_table_size(void);

#endif /* HK_PINS_H */
