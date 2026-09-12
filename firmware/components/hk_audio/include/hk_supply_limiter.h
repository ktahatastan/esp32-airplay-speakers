/**
 * @file hk_supply_limiter.h
 * @brief The adapter's current budget, expressed in the only units the DSP has.
 *
 * WHAT THIS IS FOR. The speaker runs from a 24 V / 2.9 A desktop adapter
 * (ADR-0020), and that 2.9 A feeds all four XH-A232 amplifiers -- four
 * woofers and four tweeters -- from one rail. Eight BTL channels can ask for
 * far more than the adapter will give, and what happens then is not
 * distortion: VIN sags, the buck behind the ESP32-S3 drops out, and the board
 * resets in the middle of a song. So the budget is a hard constraint on the
 * whole cabinet, and it is a constraint of a particular shape that the peak
 * limiters in this chain cannot express, for two reasons:
 *
 *   PEAK VERSUS AVERAGE. An adapter is rated in continuous current; its bulk
 *   capacitance rides out a transient. What sags the rail is the MEAN power
 *   over a window of tens of milliseconds, not the height of one sample. A
 *   peak ceiling that kept the average under budget on a sine would be far
 *   too low for music, whose crest is 12-18 dB above its mean; one that let
 *   music through at a sensible level would let a sustained sine draw many
 *   times the budget.
 *
 *   SUM VERSUS BRANCH. The peak limiters are per branch, because a tweeter's
 *   ceiling is about that tweeter. The rail does not know which driver drew
 *   the current: it sees woofers and tweeters together. A budget split into
 *   two per-branch numbers would be either unsafe when both play or needlessly
 *   quiet when one does.
 *
 * So this stage watches the MEAN SQUARE OF (woofer^2 + tweeter^2), summed over
 * both branches after their gains and before the peak limiters, through a
 * one-pole window, and when that mean exceeds a budget it applies one common
 * gain to both branches so the output's mean square comes down to the budget.
 * Common, so the crossover sum is preserved; before the peak limiters, so the
 * per-branch ceiling guarantee is untouched (hk_dsp.h, guarantee (a)).
 *
 * FEED-FORWARD, ON THE INPUT. The detector reads the signal BEFORE the gain
 * it computes is applied. With mean_sq settled at M > budget the gain is
 * sqrt(budget / M) and the output's mean square is exactly budget -- the loop
 * is open, so there is nothing for it to hunt around. A detector on the
 * output would see its own reduction, decide it was under budget, let go,
 * and pump.
 *
 * ONE SQUARE ROOT PER BLOCK. The target gain is computed once at the start of
 * each block from the detector as it stands, and the per-sample work is a
 * multiply-add on the detector and one add on the gain, slewing linearly to
 * the target across the block. That keeps the per-sample cost the same
 * whether the stage is engaged or idle, which is what lets the audio task's
 * time be reported as a number rather than a range.
 *
 * NOTHING HERE HAS A DEFAULT BUDGET. The two numbers this needs -- the budget
 * as a DAC-referred mean square, and the window -- come from G1 step S7: the
 * adapter, all four amplifiers driven into 4 ohm-class loads, VIN on a scope,
 * and the digital level raised until the rail's sag is what the record says
 * it may be. That number depends on the amplifier's gain strapping (bench
 * item C3, unread) and on the load, so it cannot be derived on paper, and it
 * is NOT scaled by supply voltage the way the peak ceilings are: it was
 * measured on the adapter, it describes the adapter, and it is stored as
 * read. A configuration without one is refused rather than run.
 */
#ifndef HK_SUPPLY_LIMITER_H
#define HK_SUPPLY_LIMITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Largest budget the validator accepts: two full-scale branches. */
#define HK_SUPPLY_LIMITER_BUDGET_MAX 2.0f

/** Settings. The budget is a mean square in linear full-scale units. */
typedef struct {
    float    budget_sq;   /**< Mean of (woofer^2 + tweeter^2) the rail allows, 0 < b <= 2 */
    uint32_t window_ms;   /**< One-pole averaging window; >= 1 */
    uint32_t sample_rate; /**< Samples per second, per branch */
} hk_supply_limiter_config_t;

/** Running state. The caller owns it; the module keeps no globals. */
typedef struct {
    hk_supply_limiter_config_t config;
    float    mean_sq; /**< The detector: one-pole mean of the input's w^2 + t^2 */
    float    coeff;   /**< Per-sample detector coefficient, exp(-1 / window samples) */
    float    gain;    /**< Current common gain, 0 < g <= 1 */
    float    target;  /**< Gain the block is slewing towards */
    float    slew;    /**< Per-sample step from gain to target, signed */
    bool     ready;   /**< Configuration was accepted */
} hk_supply_limiter_t;

/**
 * Check a configuration without building anything from it.
 *
 * A budget that is not finite, zero, negative or above two full-scale
 * branches, a window under a millisecond, or a sample rate of zero are not
 * tuning choices; they describe a stage that is silent, never engages, or
 * cannot be built.
 */
bool hk_supply_limiter_config_valid(const hk_supply_limiter_config_t *config);

/**
 * Prepare the stage: detector at zero, gain at unity.
 *
 * @return false if the configuration is not usable; the stage is then not
 *         ready and hk_supply_limiter_step() returns unity, which the caller
 *         must refuse to run on -- as hk_dsp does, once per block, before a
 *         sample moves.
 */
bool hk_supply_limiter_init(hk_supply_limiter_t *limiter,
                            const hk_supply_limiter_config_t *config);

/** Forget the signal: detector to zero, gain back to unity. Keeps the config. */
void hk_supply_limiter_reset(hk_supply_limiter_t *limiter);

/**
 * Once per block, BEFORE the block's first hk_supply_limiter_step().
 *
 * Reads the detector, decides the gain the block should end at, and divides
 * the distance by @p frames. This is the only place the square root and the
 * divide live.
 */
void hk_supply_limiter_block_begin(hk_supply_limiter_t *limiter, size_t frames);

/**
 * One frame: update the detector from the INPUT and return the common gain
 * to multiply both branches by.
 *
 * The gain moves one slew step towards the block's target and never past it.
 * A non-finite input leaves the detector and the gain untouched and returns
 * the current gain; an unready stage returns 1.0f.
 */
float hk_supply_limiter_step(hk_supply_limiter_t *limiter, float woofer, float tweeter);

#endif /* HK_SUPPLY_LIMITER_H */
