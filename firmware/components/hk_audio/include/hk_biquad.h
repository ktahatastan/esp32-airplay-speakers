/**
 * @file hk_biquad.h
 * @brief Second-order sections, and the Linkwitz-Riley pair built from them.
 *
 * The corner frequencies in this project come from G0 and G2 — from measured
 * driver impedance and measured tweeter behaviour — and none of them exists
 * yet. The MATHS does not have to wait for them, so it does not: everything
 * here takes its frequency as an argument and refuses to invent one.
 *
 * Two notes on the choices.
 *
 * DIRECT FORM II TRANSPOSED, not Direct Form I. Both are the same filter on
 * paper. In single-precision float they are not: DF2T keeps its state in the
 * same magnitude range as the signal, while DF1 accumulates the difference of
 * two large nearly-equal numbers and loses bits at low corner frequencies —
 * exactly where a tweeter's protection high-pass lives.
 *
 * LINKWITZ-RILEY 4th ORDER, built as two cascaded Butterworth sections at
 * Q = 1/sqrt(2). The reason to prefer it over a plain Butterworth pair is the
 * one property that matters for a two-way speaker: the low and high branches
 * SUM FLAT. A Butterworth pair is 3 dB up at the crossover; an LR4 pair is
 * 6 dB down on each branch and adds back to unity. And unlike LR2, the two
 * LR4 branches are in phase, so neither needs its polarity inverted — a
 * standing trap, because inverting one anyway produces a deep notch at the
 * crossover that measures as "a crossover problem" rather than as a wiring
 * mistake.
 */
#ifndef HK_BIQUAD_H
#define HK_BIQUAD_H

#include <stdbool.h>
#include <stddef.h>

/** Normalised coefficients: a0 has already been divided out. */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
} hk_biquad_coeffs_t;

/** Filter memory for one section. Zero it to start from silence. */
typedef struct {
    float z1, z2;
} hk_biquad_state_t;

/**
 * Lowest corner this design will produce, as a fraction of the sample rate.
 *
 * Below roughly fc/fs = 4.83e-5 the coefficients stop describing a stable
 * filter in single precision: a2 approaches 1 from below, and the rounding
 * pushes the poles onto or outside the unit circle. Measured by bisection at
 * 44.1, 48, 96 and 192 kHz, which all break at the same ratio because it is a
 * ratio.
 *
 * The limit here is about twice that, which is 4.8 Hz at 48 kHz. Everything
 * this project needs sits far above it — a 2 kHz crossover is 0.042, a
 * tweeter's 80 Hz protection high-pass is 0.0017, a 20 Hz subsonic filter is
 * 4.2e-4 — so the margin costs nothing and the alternative is a filter that
 * quietly rings instead of one that was never built.
 */
#define HK_BIQUAD_MIN_FC_RATIO 1.0e-4f

/** Butterworth-family Q. Both Linkwitz-Riley sections use it. */
#define HK_BIQUAD_Q_BUTTERWORTH 0.70710678f

/**
 * Design a second-order low-pass.
 *
 * @return false if the request cannot be met: a corner at or above Nyquist has
 *         no meaning, a corner below ::HK_BIQUAD_MIN_FC_RATIO cannot be
 *         represented stably in single precision, and a non-positive
 *         frequency, sample rate or Q produces coefficients that are not a
 *         filter. Refused rather than approximated, because a filter that
 *         quietly became something else is worse than one that was never
 *         built.
 */
bool hk_biquad_lowpass(hk_biquad_coeffs_t *coeffs, float fc_hz, float fs_hz, float q);

/** Design a second-order high-pass. Same refusals as hk_biquad_lowpass(). */
bool hk_biquad_highpass(hk_biquad_coeffs_t *coeffs, float fc_hz, float fs_hz, float q);

/** One sample through one section. */
float hk_biquad_process_one(const hk_biquad_coeffs_t *coeffs,
                            hk_biquad_state_t *state, float sample);

/** A block through one section, in place. */
void hk_biquad_process(const hk_biquad_coeffs_t *coeffs,
                       hk_biquad_state_t *state, float *samples, size_t count);

/**
 * Whether a set of coefficients describes a stable filter.
 *
 * Both poles must sit inside the unit circle, which for a normalised biquad is
 * |a2| < 1 and |a1| < 1 + a2. Designed sections always pass; this exists for
 * coefficients that arrive from the calibration store, where nothing has
 * guaranteed anything.
 */
bool hk_biquad_stable(const hk_biquad_coeffs_t *coeffs);

/** A 4th-order Linkwitz-Riley branch: two identical Butterworth sections. */
typedef struct {
    hk_biquad_coeffs_t section[2];
} hk_lr4_coeffs_t;

/** State for one ::hk_lr4_coeffs_t branch. */
typedef struct {
    hk_biquad_state_t section[2];
} hk_lr4_state_t;

/** Design the low branch of an LR4 crossover at @p fc_hz. */
bool hk_lr4_lowpass(hk_lr4_coeffs_t *filter, float fc_hz, float fs_hz);

/** Design the high branch. In phase with the low branch: do not invert it. */
bool hk_lr4_highpass(hk_lr4_coeffs_t *filter, float fc_hz, float fs_hz);

/** One sample through both sections. */
float hk_lr4_process_one(const hk_lr4_coeffs_t *filter,
                         hk_lr4_state_t *state, float sample);

/** A block through both sections, in place. */
void hk_lr4_process(const hk_lr4_coeffs_t *filter,
                    hk_lr4_state_t *state, float *samples, size_t count);

#endif /* HK_BIQUAD_H */
