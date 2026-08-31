/**
 * @file hk_power.h
 * @brief What the pack voltage and cell temperature mean, and what to do about it.
 *
 * Three ideas shape this module, and each exists because the obvious
 * alternative fails in a way nobody would notice until it mattered.
 *
 * An IMPLAUSIBLE READING IS NOT A FLAT BATTERY. A 4S pack cannot read 0 mV or
 * 25 V. Those numbers mean a divider came unstuck or the ADC never got
 * connected, and the right response is to stop trusting the number — not to
 * shut a speaker down mid-song because a resistor fell off, and not to carry
 * on as though the pack were fine. So an implausible reading produces its own
 * state, and the caller is told the sensor is wrong rather than the battery.
 *
 * WORSE IS IMMEDIATE, BETTER IS EARNED. A pack sags under load and recovers
 * when the music stops, so a plain threshold comparison oscillates: the LED
 * blinks between two warnings and a shutdown starts and un-starts. Falling to
 * a worse state happens the moment the threshold is crossed, because a flat
 * battery does not wait. Climbing back needs the threshold plus a margin.
 *
 * NOTHING HERE HAS A DEFAULT. Every millivolt and every degree comes from a
 * G3/G4 measurement that has not been taken. Inventing one would produce a
 * device that looks calibrated, which is the same reasoning hk_gate and
 * hk_storage already follow. Pass the limits in; a NULL means the device
 * cannot judge itself and says so.
 *
 * Pure C, host-tested. The ADC driver and the shutdown itself belong to the
 * ESP-IDF layer.
 */
#ifndef HK_POWER_H
#define HK_POWER_H

#include <stdbool.h>
#include <stdint.h>

/** Sentinels for readings the device does not have. */
#define HK_POWER_MV_UNKNOWN   0u
#define HK_POWER_C_UNKNOWN    INT16_MIN

/** What the sensors say right now. */
typedef struct {
    uint32_t pack_mv;   /**< 4S pack terminal voltage; HK_POWER_MV_UNKNOWN if unread */
    int16_t  cell_c;    /**< Cell temperature; HK_POWER_C_UNKNOWN if unread */
    bool     charging;  /**< External power present */
} hk_power_inputs_t;

/**
 * Thresholds, from the calibration store.
 *
 * @c implausible_low_mv and @c implausible_high_mv bracket what a 4S pack can
 * physically read. They are not battery thresholds: they decide whether the
 * NUMBER is worth believing at all, before any comparison against the others.
 *
 * @c recover_margin_mv is what a reading must clear a threshold BY before the
 * state improves. Zero would be legal arithmetic and wrong behaviour.
 */
typedef struct {
    uint32_t implausible_low_mv;
    uint32_t implausible_high_mv;
    uint32_t low_mv;         /**< Warn the user below this */
    uint32_t critical_mv;    /**< Stop audio below this */
    uint32_t shutdown_mv;    /**< Shut down below this */
    uint32_t recover_margin_mv;
    int16_t  max_cell_c;     /**< Above this, treat as a thermal fault */
} hk_power_limits_t;

/** How the device is doing on power. Ordered: later is worse. */
typedef enum {
    HK_POWER_UNKNOWN = 0,   /**< No trustworthy reading. Judge nothing. */
    HK_POWER_NORMAL,
    HK_POWER_LOW,           /**< Warn, keep playing */
    HK_POWER_CRITICAL,      /**< Stop audio, prepare to shut down */
    HK_POWER_SHUTDOWN,      /**< Shut down now */
    HK_POWER_SENSOR_FAULT,  /**< The reading is impossible: the sensor is wrong */
    HK_POWER_OVERHEAT,      /**< Cell temperature above its limit */
} hk_power_state_t;

/**
 * Work out the new state from the previous one and the current readings.
 *
 * Pure: the caller keeps the state, so a test can drive any sequence without
 * waiting for a battery to discharge.
 *
 * @param previous the last state this returned; HK_POWER_UNKNOWN on first call
 * @param inputs   current readings; NULL yields HK_POWER_UNKNOWN
 * @param limits   calibrated thresholds; NULL yields HK_POWER_UNKNOWN
 */
hk_power_state_t hk_power_evaluate(hk_power_state_t previous,
                                   const hk_power_inputs_t *inputs,
                                   const hk_power_limits_t *limits);

/**
 * Whether the amplifier may produce sound.
 *
 * Folds in the charge lock from ADR-0004: V1 does not play while charging, and
 * that is a product decision rather than a consequence of the voltage. It is
 * enforced here rather than left to the caller because "do not play while
 * charging" and "do not play on a flat pack" are the same question asked twice,
 * and answering it in one place is how they stay consistent.
 *
 * @param state    the current power state
 * @param charging external power present
 */
bool hk_power_audio_permitted(hk_power_state_t state, bool charging);

/**
 * Whether the limits themselves are usable.
 *
 * Thresholds have to be ordered — shutdown below critical below low, both
 * inside the plausible bracket — or the state machine cannot express what they
 * ask for. A mis-ordered set is a calibration bug, and it should be caught
 * where it is written rather than showing up as a device that never warns.
 */
bool hk_power_limits_sane(const hk_power_limits_t *limits);

/** Short name, for logs and tests. */
const char *hk_power_state_name(hk_power_state_t state);

#endif /* HK_POWER_H */
