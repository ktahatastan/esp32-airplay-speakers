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

/** Number of GPIOs this design drives: 3 I2S + 1 button + 3 RGB + 2 I2C. */
#define HK_PIN_COUNT 9

/** Every assigned pin, as a bit mask. */
#define HK_PIN_MASK ( \
      (1ULL << HK_PIN_I2S_BCLK)  | (1ULL << HK_PIN_I2S_LRCLK) | \
      (1ULL << HK_PIN_I2S_DATA)  | (1ULL << HK_PIN_BUTTON)    | \
      (1ULL << HK_PIN_LED_R)     | (1ULL << HK_PIN_LED_G)     | \
      (1ULL << HK_PIN_LED_B)     | (1ULL << HK_PIN_I2C_SDA)   | \
      (1ULL << HK_PIN_I2C_SCL))

/**
 * Pins this design must never claim.
 *
 * GPIO0, GPIO3, GPIO45 and GPIO46 are strapping pins: driving one at reset
 * selects a boot mode, flash voltage or ROM log setting. GPIO19 and GPIO20 are
 * the native USB D-/D+ pair, which the documented USB/UART recovery path needs
 * to stay usable in every release.
 */
#define HK_PIN_FORBIDDEN_MASK ( \
      (1ULL << 0) | (1ULL << 3) | (1ULL << 45) | (1ULL << 46) | \
      (1ULL << 19) | (1ULL << 20))

_Static_assert(__builtin_popcountll(HK_PIN_MASK) == HK_PIN_COUNT,
               "hk_pins: two functions share a GPIO, or HK_PIN_COUNT is stale");
_Static_assert((HK_PIN_MASK & HK_PIN_FORBIDDEN_MASK) == 0,
               "hk_pins: an assignment lands on a strapping or native-USB pin");
_Static_assert((HK_PIN_MASK >> (HK_GPIO_MAX + 1)) == 0,
               "hk_pins: an assignment exceeds the highest ESP32-S3 GPIO");

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
