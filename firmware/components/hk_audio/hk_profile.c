#include "hk_profile.h"

#include <math.h>
#include <string.h>

/** Finite and strictly positive. NaN fails every comparison, so ask directly. */
static bool positive(float value)
{
    return isfinite(value) && value > 0.0f;
}

/** A linear gain or ceiling: above zero and no more than full scale. */
static bool unit_range(float value)
{
    return positive(value) && value <= 1.0f;
}

hk_profile_verdict_t hk_profile_valid(const hk_profile_t *profile)
{
    if (profile == NULL) {
        return HK_PROFILE_BAD_SCHEMA;
    }
    if (profile->schema != HK_PROFILE_SCHEMA) {
        return HK_PROFILE_BAD_SCHEMA;
    }

    /* memchr rather than strlen: an unterminated array would make every later
     * reader run off the end of the struct. */
    if (profile->source[0] == '\0' ||
        memchr(profile->source, '\0', sizeof(profile->source)) == NULL ||
        profile->measured_yyyymmdd == 0u) {
        return HK_PROFILE_NO_SOURCE;
    }

    /* Carried, not computed with -- but a profile that cannot say what it was
     * derived from is the guess this project refuses to run on. */
    if (!positive(profile->woofer_dcr_ohm) || !positive(profile->tweeter_dcr_ohm)) {
        return HK_PROFILE_BAD_MEASUREMENT;
    }

    if (!positive(profile->woofer_hpf_hz) || !positive(profile->crossover_hz)) {
        return HK_PROFILE_BAD_FREQUENCY;
    }
    /* A subsonic filter above the crossover is not a subsonic filter; it is two
     * numbers that were swapped, and the result would be a woofer branch with
     * nothing left in it. */
    if (profile->woofer_hpf_hz >= profile->crossover_hz) {
        return HK_PROFILE_BAD_FREQUENCY;
    }

    if (!unit_range(profile->woofer_gain) || !unit_range(profile->tweeter_gain)) {
        return HK_PROFILE_BAD_GAIN;
    }
    if (!unit_range(profile->woofer_ceiling) || !unit_range(profile->tweeter_ceiling)) {
        return HK_PROFILE_BAD_CEILING;
    }

    if (!isfinite(profile->reference_supply_mv) ||
        profile->reference_supply_mv < HK_PROFILE_SUPPLY_MV_MIN ||
        profile->reference_supply_mv > HK_PROFILE_SUPPLY_MV_MAX) {
        return HK_PROFILE_BAD_REFERENCE;
    }

    /* Zero release is not a fast limiter, it is a gate. hk_limiter refuses it
     * too; catching it here names the field instead of the stage. */
    if (profile->release_ms == 0u) {
        return HK_PROFILE_BAD_TIMING;
    }

    return HK_PROFILE_OK;
}

hk_profile_verdict_t hk_profile_from_blob(const void *blob, size_t length,
                                          hk_profile_t *out)
{
    if (blob == NULL || out == NULL || length != sizeof(hk_profile_t)) {
        return HK_PROFILE_BAD_SCHEMA;
    }
    memcpy(out, blob, sizeof(*out));

    const hk_profile_verdict_t verdict = hk_profile_valid(out);
    if (verdict != HK_PROFILE_OK) {
        /* Nothing half-read is left for a caller that ignores the verdict. */
        memset(out, 0, sizeof(*out));
    }
    return verdict;
}

float hk_profile_ceiling_at(float ceiling_at_reference, float reference_mv, float supply_mv)
{
    if (!unit_range(ceiling_at_reference) || !positive(reference_mv) || !positive(supply_mv)) {
        return 0.0f;
    }

    const float scaled = ceiling_at_reference * (reference_mv / supply_mv);
    if (!isfinite(scaled)) {
        return 0.0f;
    }
    /* Full scale is the end of the signal, not a tuning limit: past it there is
     * simply nothing more to send, so the clamp is arithmetic rather than a
     * safety decision. The unsafe direction -- a supply ABOVE the reference --
     * lands below the stored ceiling, which is the whole point. */
    return (scaled > 1.0f) ? 1.0f : scaled;
}

hk_profile_verdict_t hk_profile_build(const hk_profile_t *profile,
                                      float fs_hz, float supply_mv,
                                      hk_profile_chain_t *out)
{
    if (out == NULL) {
        return HK_PROFILE_UNBUILDABLE;
    }
    memset(out, 0, sizeof(*out));

    const hk_profile_verdict_t verdict = hk_profile_valid(profile);
    if (verdict != HK_PROFILE_OK) {
        return verdict;
    }
    if (!positive(fs_hz) || !positive(supply_mv)) {
        return HK_PROFILE_UNBUILDABLE;
    }

    /* The subsonic filter is one second-order section, not an LR4 pair: nothing
     * is crossed over here, there is only content below it that the woofer
     * would turn into excursion instead of sound. Butterworth Q. */
    if (!hk_biquad_highpass(&out->woofer_hpf, profile->woofer_hpf_hz, fs_hz, 0.70710678f)) {
        return HK_PROFILE_UNBUILDABLE;
    }
    if (!hk_lr4_lowpass(&out->woofer_low, profile->crossover_hz, fs_hz)) {
        return HK_PROFILE_UNBUILDABLE;
    }
    if (!hk_lr4_highpass(&out->tweeter_high, profile->crossover_hz, fs_hz)) {
        return HK_PROFILE_UNBUILDABLE;
    }

    out->woofer_gain = profile->woofer_gain;
    out->tweeter_gain = profile->tweeter_gain;

    const float woofer_ceiling = hk_profile_ceiling_at(profile->woofer_ceiling,
                                                       profile->reference_supply_mv, supply_mv);
    const float tweeter_ceiling = hk_profile_ceiling_at(profile->tweeter_ceiling,
                                                        profile->reference_supply_mv, supply_mv);
    if (woofer_ceiling <= 0.0f || tweeter_ceiling <= 0.0f) {
        return HK_PROFILE_UNBUILDABLE;
    }

    const uint32_t rate = (uint32_t)fs_hz;
    out->woofer_limit = (hk_limiter_config_t){
        .ceiling     = woofer_ceiling,
        .release_ms  = profile->release_ms,
        .hold_ms     = profile->hold_ms,
        .sample_rate = rate,
    };
    out->tweeter_limit = (hk_limiter_config_t){
        .ceiling     = tweeter_ceiling,
        .release_ms  = profile->release_ms,
        .hold_ms     = profile->hold_ms,
        .sample_rate = rate,
    };

    /* Asked rather than assumed. The limiter has its own refusals and they are
     * allowed to be stricter than the ones above; a config this module accepted
     * and that module rejects would surface as a stage that never engages. */
    if (!hk_limiter_config_valid(&out->woofer_limit) ||
        !hk_limiter_config_valid(&out->tweeter_limit)) {
        memset(out, 0, sizeof(*out));
        return HK_PROFILE_UNBUILDABLE;
    }

    return HK_PROFILE_OK;
}

const char *hk_profile_verdict_name(hk_profile_verdict_t verdict)
{
    switch (verdict) {
    case HK_PROFILE_OK:              return "ok";
    case HK_PROFILE_BAD_SCHEMA:      return "schema";
    case HK_PROFILE_NO_SOURCE:       return "source";
    case HK_PROFILE_BAD_MEASUREMENT: return "measurement";
    case HK_PROFILE_BAD_FREQUENCY:   return "frequency";
    case HK_PROFILE_BAD_GAIN:        return "gain";
    case HK_PROFILE_BAD_CEILING:     return "ceiling";
    case HK_PROFILE_BAD_REFERENCE:   return "reference";
    case HK_PROFILE_BAD_TIMING:      return "timing";
    case HK_PROFILE_UNBUILDABLE:     return "unbuildable";
    default:                         return "unknown";
    }
}
