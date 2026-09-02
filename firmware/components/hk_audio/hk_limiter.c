#include "hk_limiter.h"

#include <math.h>
#include <stddef.h>

bool hk_limiter_config_valid(const hk_limiter_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    /* A ceiling at or below zero silences the output; above full scale it
     * never engages. Neither is a tuning choice. */
    if (!(config->ceiling > 0.0f) || config->ceiling > 1.0f) {
        return false;
    }
    /* NaN fails the comparison above, but say so explicitly: an unset float is
     * a likely way for this to arrive wrong. */
    if (isnan(config->ceiling)) {
        return false;
    }
    if (config->sample_rate == 0u) {
        return false;
    }
    /* Zero release turns the limiter into a switch and produces the pumping it
     * exists to avoid. */
    if (config->release_ms == 0u) {
        return false;
    }
    return true;
}

bool hk_limiter_init(hk_limiter_t *limiter, const hk_limiter_config_t *config)
{
    if (limiter == NULL) {
        return false;
    }
    limiter->ready = false;
    limiter->gain = 1.0f;
    limiter->held = 0u;

    if (!hk_limiter_config_valid(config)) {
        return false;
    }

    limiter->config = *config;

    /* One-pole exponential recovery: after release_ms the gain has closed
     * about 63% of the distance back to unity. */
    const float samples = ((float)config->release_ms / 1000.0f) *
                          (float)config->sample_rate;
    limiter->release_coeff = (samples >= 1.0f) ? expf(-1.0f / samples) : 0.0f;

    /* The recovery recurrence only stays below unity while the coefficient
     * does. Checked here rather than clamped per sample, so a change to the
     * maths above fails loudly instead of being silently corrected a million
     * times a second. */
    if (!(limiter->release_coeff >= 0.0f) || limiter->release_coeff >= 1.0f) {
        return false;
    }

    limiter->hold_samples = (uint32_t)(((float)config->hold_ms / 1000.0f) *
                                       (float)config->sample_rate);
    limiter->ready = true;
    return true;
}

float hk_limiter_step(hk_limiter_t *limiter, float peak)
{
    if (limiter == NULL || !limiter->ready) {
        return 1.0f;
    }

    /* A broken sample must not become a broken gain that then multiplies every
     * sample after it. This guard used to test isnan alone, which let an
     * infinity through: fabsf(INFINITY) exceeds any ceiling, so the reduction
     * ran and set gain = ceiling / INFINITY = 0, and the hold kept it there.
     * One bad sample silenced the output for the whole hold time — measured at
     * about 10 ms — which is the opposite of the guarantee this module makes.
     *
     * Leave the state alone instead. The sample itself is already broken and
     * this stage is not the place to repair it. */
    if (!isfinite(peak)) {
        return limiter->gain;
    }

    const float magnitude = fabsf(peak);

    /* Recovery happens FIRST, and the ceiling check runs against the gain that
     * results. Doing it the other way round — deciding no reduction is needed,
     * then letting the gain recover, then applying that larger gain to the
     * same sample — lets the output past the ceiling by exactly the amount the
     * gain just recovered. Small, silent, and a direct breach of the one
     * guarantee this stage makes. */
    if (limiter->held > 0u) {
        limiter->held--;
    } else if (limiter->gain < 1.0f) {
        /* Recovers towards unity and cannot pass it: with gain < 1 and
         * release_coeff in [0,1), the result is strictly below 1. There is no
         * clamp here because there was one, and no test could tell whether it
         * worked — unreachable code on a safety path is a comfort, not a
         * guard. What makes it unreachable is the coefficient range, so that
         * is checked once at init instead, where a change to the maths would
         * be caught. */
        limiter->gain = 1.0f - (1.0f - limiter->gain) * limiter->release_coeff;
    }

    if (magnitude * limiter->gain > limiter->config.ceiling) {
        /* Exactly the gain that brings THIS sample to the ceiling, applied to
         * THIS sample. No attack time, so nothing gets through above the
         * ceiling while the gain is still on its way down. */
        limiter->gain = limiter->config.ceiling / magnitude;
        limiter->held = limiter->hold_samples;
    }

    return limiter->gain;
}

bool hk_limiter_process(hk_limiter_t *limiter, float *samples, size_t count)
{
    if (limiter == NULL || !limiter->ready) {
        return false;
    }
    if (samples == NULL) {
        return count == 0u;
    }

    for (size_t i = 0; i < count; i++) {
        samples[i] *= hk_limiter_step(limiter, samples[i]);
    }
    return true;
}
