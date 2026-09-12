#include "hk_biquad.h"

#include <math.h>
#include <stddef.h>

/** Shared refusals: a request that cannot describe a filter. */
static bool request_valid(float fc_hz, float fs_hz, float q)
{
    /* isfinite rather than isnan: an infinite Q passed the old check, and
     * sinf(w0)/(2*INFINITY) is zero, so alpha vanished and a2 came out exactly
     * 1 — a section with its poles on the unit circle, which the stability
     * check in this same file then rejects. Refusing the request is better
     * than returning coefficients the module itself calls unstable. */
    if (!isfinite(fc_hz) || !isfinite(fs_hz) || !isfinite(q)) {
        return false;
    }
    if (!(fs_hz > 0.0f) || !(q > 0.0f) || !(fc_hz > 0.0f)) {
        return false;
    }
    /* At or above Nyquist there is no such filter to design. */
    if (fc_hz >= fs_hz / 2.0f) {
        return false;
    }
    /* And below a certain fraction of the sample rate there is no such filter
     * to design IN FLOAT. The coefficients still come out, and they still look
     * like a filter, but a2 has rounded to where the poles are no longer
     * inside the unit circle. Returning them would hand the caller a section
     * that rings rather than settles, on the tweeter protection path. */
    if (fc_hz < fs_hz * HK_BIQUAD_MIN_FC_RATIO) {
        return false;
    }
    return true;
}

bool hk_biquad_lowpass(hk_biquad_coeffs_t *coeffs, float fc_hz, float fs_hz, float q)
{
    if (coeffs == NULL || !request_valid(fc_hz, fs_hz, q)) {
        return false;
    }

    const float w0 = 2.0f * (float)M_PI * fc_hz / fs_hz;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;

    coeffs->b0 = ((1.0f - cos_w0) / 2.0f) / a0;
    coeffs->b1 = (1.0f - cos_w0) / a0;
    coeffs->b2 = coeffs->b0;
    coeffs->a1 = (-2.0f * cos_w0) / a0;
    coeffs->a2 = (1.0f - alpha) / a0;
    return true;
}

bool hk_biquad_highpass(hk_biquad_coeffs_t *coeffs, float fc_hz, float fs_hz, float q)
{
    if (coeffs == NULL || !request_valid(fc_hz, fs_hz, q)) {
        return false;
    }

    const float w0 = 2.0f * (float)M_PI * fc_hz / fs_hz;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;

    coeffs->b0 = ((1.0f + cos_w0) / 2.0f) / a0;
    coeffs->b1 = (-(1.0f + cos_w0)) / a0;
    coeffs->b2 = coeffs->b0;
    coeffs->a1 = (-2.0f * cos_w0) / a0;
    coeffs->a2 = (1.0f - alpha) / a0;
    return true;
}

bool hk_biquad_stable(const hk_biquad_coeffs_t *coeffs)
{
    if (coeffs == NULL) {
        return false;
    }
    /* All five, not just the poles. This used to test isnan on a1 and a2
     * alone, which let a NaN or infinite b0 through: Jury's criterion is a
     * statement about the denominator and says nothing about the numerator,
     * so a section whose feed-forward path was garbage counted as stable,
     * ran, and turned every sample into NaN that the output stage clamped to
     * zero -- silence with no refusal to name it. */
    if (!isfinite(coeffs->b0) || !isfinite(coeffs->b1) || !isfinite(coeffs->b2) ||
        !isfinite(coeffs->a1) || !isfinite(coeffs->a2)) {
        return false;
    }
    /* Jury's criterion for a second-order section: both poles inside the unit
     * circle. */
    return fabsf(coeffs->a2) < 1.0f && fabsf(coeffs->a1) < 1.0f + coeffs->a2;
}

float hk_biquad_process_one(const hk_biquad_coeffs_t *coeffs,
                            hk_biquad_state_t *state, float sample)
{
    if (coeffs == NULL || state == NULL) {
        return sample;
    }
    /* Direct Form II transposed. */
    const float y = coeffs->b0 * sample + state->z1;
    state->z1 = coeffs->b1 * sample - coeffs->a1 * y + state->z2;
    state->z2 = coeffs->b2 * sample - coeffs->a2 * y;
    return y;
}

void hk_biquad_process(const hk_biquad_coeffs_t *coeffs,
                       hk_biquad_state_t *state, float *samples, size_t count)
{
    if (coeffs == NULL || state == NULL || samples == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        samples[i] = hk_biquad_process_one(coeffs, state, samples[i]);
    }
}

bool hk_lr4_lowpass(hk_lr4_coeffs_t *filter, float fc_hz, float fs_hz)
{
    if (filter == NULL) {
        return false;
    }
    if (!hk_biquad_lowpass(&filter->section[0], fc_hz, fs_hz,
                           HK_BIQUAD_Q_BUTTERWORTH)) {
        return false;
    }
    filter->section[1] = filter->section[0];
    return true;
}

bool hk_lr4_highpass(hk_lr4_coeffs_t *filter, float fc_hz, float fs_hz)
{
    if (filter == NULL) {
        return false;
    }
    if (!hk_biquad_highpass(&filter->section[0], fc_hz, fs_hz,
                            HK_BIQUAD_Q_BUTTERWORTH)) {
        return false;
    }
    filter->section[1] = filter->section[0];
    return true;
}

bool hk_butterworth4_highpass(hk_lr4_coeffs_t *filter, float fc_hz, float fs_hz)
{
    if (filter == NULL) {
        return false;
    }
    /* Two sections at the same corner and DIFFERENT Qs. Cascading a pair of
     * Q = 0.7071 sections gives LR4, whose response at fc is -6 dB; the
     * fourth-order Butterworth's poles lie on the unit circle at 22.5 and
     * 67.5 degrees from the negative real axis, and those pairs are
     * Q = 1/(2 cos 22.5) = 0.5412 and Q = 1/(2 cos 67.5) = 1.3066. Their
     * product is 0.7071, which is what puts the cascade back at -3 dB at fc.
     * Both requests share one corner, so if the first is refused the second
     * would be too; the order of the two calls is only which section's
     * memory holds the peakier transient. */
    if (!hk_biquad_highpass(&filter->section[0], fc_hz, fs_hz,
                            HK_BIQUAD_Q_BUTTERWORTH4_A)) {
        return false;
    }
    if (!hk_biquad_highpass(&filter->section[1], fc_hz, fs_hz,
                            HK_BIQUAD_Q_BUTTERWORTH4_B)) {
        return false;
    }
    return true;
}

float hk_lr4_process_one(const hk_lr4_coeffs_t *filter,
                         hk_lr4_state_t *state, float sample)
{
    if (filter == NULL || state == NULL) {
        return sample;
    }
    float y = hk_biquad_process_one(&filter->section[0], &state->section[0], sample);
    return hk_biquad_process_one(&filter->section[1], &state->section[1], y);
}

void hk_lr4_process(const hk_lr4_coeffs_t *filter,
                    hk_lr4_state_t *state, float *samples, size_t count)
{
    if (filter == NULL || state == NULL || samples == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        samples[i] = hk_lr4_process_one(filter, state, samples[i]);
    }
}
