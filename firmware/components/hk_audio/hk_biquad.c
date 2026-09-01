#include "hk_biquad.h"

#include <math.h>
#include <stddef.h>

/** Shared refusals: a request that cannot describe a filter. */
static bool request_valid(float fc_hz, float fs_hz, float q)
{
    if (isnan(fc_hz) || isnan(fs_hz) || isnan(q)) {
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
    if (isnan(coeffs->a1) || isnan(coeffs->a2)) {
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
