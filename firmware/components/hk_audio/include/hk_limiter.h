/**
 * @file hk_limiter.h
 * @brief A peak limiter whose job is protection, not loudness.
 *
 * This exists to keep a tweeter alive, not to make anything sound louder. That
 * single sentence settles most of the design choices below, because the usual
 * trade-offs in a mastering limiter point the other way.
 *
 * ZERO ATTACK, NO LOOKAHEAD. The gain needed to bring a sample under the
 * ceiling is computed from that sample and applied to it, so |output| never
 * exceeds the ceiling — no overshoot, ever. The textbook alternative smooths
 * the gain reduction over an attack time, which sounds gentler and lets peaks
 * through while the gain is still falling; a limiter that lets peaks through
 * is not protection. The other alternative, a lookahead delay line, removes
 * the distortion but adds latency — and this device has a latency budget it
 * cannot spend, because ADR-0007 gives group synchronisation ≤1 ms between
 * rooms. A few milliseconds of lookahead would eat that budget to make a
 * protection stage that rarely engages sound nicer while it engages.
 *
 * The cost is honest: instantaneous gain changes are distortion. On a stage
 * that should be inaudible in normal use and only acts when something is
 * already wrong, that is the right way round.
 *
 * SMOOTHED RELEASE AND A HOLD. Recovery is the opposite case. Letting the gain
 * snap back produces pumping, and letting it recover during a brief dip
 * between two loud transients means the second transient arrives at full gain.
 * So release is exponential and a hold keeps the gain down for a stated time
 * after the last peak.
 *
 * NOTHING HERE HAS A DEFAULT CEILING. The ceiling comes from G2, from measured
 * driver behaviour, and this project does not invent that kind of number. A
 * configuration that has not been given one is refused rather than run.
 */
#ifndef HK_LIMITER_H
#define HK_LIMITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Settings. All times in milliseconds; ceiling in linear full-scale units. */
typedef struct {
    float    ceiling;      /**< Absolute peak the output may reach, 0 < c <= 1 */
    uint32_t release_ms;   /**< Time to recover ~63% of the gain after hold */
    uint32_t hold_ms;      /**< Gain stays down this long after the last peak */
    uint32_t sample_rate;  /**< Samples per second, per channel */
} hk_limiter_config_t;

/** Running state. The caller owns it; the module keeps no globals. */
typedef struct {
    hk_limiter_config_t config;
    float    gain;          /**< Current gain, 0 < g <= 1 */
    float    release_coeff; /**< Per-sample exponential recovery factor */
    uint32_t hold_samples;  /**< Hold length, in samples */
    uint32_t held;          /**< Samples remaining in the current hold */
    bool     ready;         /**< Configuration was accepted */
} hk_limiter_t;

/**
 * Check a configuration without building anything from it.
 *
 * A ceiling of zero or above full scale, a sample rate of zero, or a release
 * time of zero are not tuning choices — they are values that make the limiter
 * either silent, undefined or a switch. They are refused where they are
 * written rather than producing a stage that behaves strangely.
 */
bool hk_limiter_config_valid(const hk_limiter_config_t *config);

/**
 * Prepare a limiter.
 *
 * @return false if the configuration is not usable; the limiter is then marked
 *         not ready and hk_limiter_process() will refuse to run rather than
 *         pass audio through unprotected.
 */
bool hk_limiter_init(hk_limiter_t *limiter, const hk_limiter_config_t *config);

/**
 * Limit @p count samples in place.
 *
 * @return true if the block was processed. False means the limiter was never
 *         configured, and the caller must treat that as "do not play" rather
 *         than "play unprotected" — the entire reason this stage exists is
 *         that the driver cannot survive the alternative.
 */
bool hk_limiter_process(hk_limiter_t *limiter, float *samples, size_t count);

/**
 * The gain that would be applied to a sample of magnitude @p peak, and the
 * state update that goes with it.
 *
 * Exposed because it is the whole algorithm, and testing it directly is how
 * the ceiling guarantee gets checked across values that would take a long
 * signal to reach otherwise.
 */
float hk_limiter_step(hk_limiter_t *limiter, float peak);

#endif /* HK_LIMITER_H */
