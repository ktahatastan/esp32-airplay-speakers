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

/** One of the TPA3110D2's four strap settings (SLOS528F Table 2), or unread. */
static bool amp_gain_known_or_unread(uint32_t gain_db)
{
    return gain_db == 0u || gain_db == 20u || gain_db == 26u ||
           gain_db == 32u || gain_db == 36u;
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
     * too; catching it here names the field instead of the stage. Each branch
     * has its own pair now, and each is judged on its own. */
    if (profile->woofer_release_ms == 0u || profile->tweeter_release_ms == 0u) {
        return HK_PROFILE_BAD_TIMING;
    }

    /* An alignment delay is a correction of one branch RELATIVE to the other,
     * so delaying both is a latency nobody asked for wearing a correction's
     * name. And it is bounded: past the bound it is no longer an acoustic
     * centre offset between two drivers in one baffle. */
    if (profile->woofer_delay_samples > HK_PROFILE_DELAY_MAX_SAMPLES ||
        profile->tweeter_delay_samples > HK_PROFILE_DELAY_MAX_SAMPLES ||
        (profile->woofer_delay_samples != 0u && profile->tweeter_delay_samples != 0u)) {
        return HK_PROFILE_BAD_DELAY;
    }

    if (profile->tweeter_polarity > 1u) {
        return HK_PROFILE_BAD_POLARITY;
    }

    /* The adapter budget is refused on the same terms as a ceiling: absent,
     * impossible, or claiming more than two full-scale branches can carry.
     * hk_supply_limiter refuses the same values; naming the field here is
     * what makes a bench report say "budget" rather than "unbuildable". */
    if (!isfinite(profile->supply_budget_sq) ||
        !(profile->supply_budget_sq > 0.0f) ||
        profile->supply_budget_sq > HK_PROFILE_SUPPLY_BUDGET_MAX ||
        profile->supply_window_ms == 0u) {
        return HK_PROFILE_BAD_BUDGET;
    }

    /* Provenance, not a control: zero means C3 has not been done, which is
     * accepted -- whether an unread strap should block a factory write is the
     * owner's call and is not taken here. A value that is none of the
     * amplifier's settings is a transcription error, and that is refused. */
    if (!amp_gain_known_or_unread(profile->amp_gain_db)) {
        return HK_PROFILE_BAD_AMP_GAIN;
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

    /* One direction only. A supply above the reference scales the ceiling
     * down, which errs quiet. A supply below it does NOT scale up: the
     * amplifier's gain is fixed, so the volts a digital level produces do not
     * fall with the rail -- the rail's clip point does -- and a ceiling
     * raised on that reasoning would be the one unsafe direction this
     * function could take. The stored value is the answer there. */
    const float ratio = reference_mv / supply_mv;
    if (!isfinite(ratio)) {
        return 0.0f;
    }
    if (ratio >= 1.0f) {
        return ceiling_at_reference;
    }
    const float scaled = ceiling_at_reference * ratio;
    return isfinite(scaled) ? scaled : 0.0f;
}

/** The build proper; the wrapper below zeroes @p out on any refusal. */
static hk_profile_verdict_t build_chain(const hk_profile_t *profile,
                                        float fs_hz, float supply_mv,
                                        hk_profile_chain_t *out)
{
    const hk_profile_verdict_t verdict = hk_profile_valid(profile);
    if (verdict != HK_PROFILE_OK) {
        return verdict;
    }
    if (!positive(fs_hz) || !positive(supply_mv)) {
        return HK_PROFILE_UNBUILDABLE;
    }

    /* The subsonic filter is a fourth-order Butterworth, not an LR4 pair:
     * nothing is crossed over here, there is only content below it that the
     * woofer would turn into excursion instead of sound, so the flat-sum
     * property buys nothing and the -3 dB corner is the one the field names.
     * Fourth order because a second-order section is only 12 dB down an
     * octave below its corner, and below resonance a cone's excursion for
     * the same output keeps rising with every octave; 24 dB/octave is what
     * the record asked for (driver-measurements.md, ADR-0002) and what the
     * crossover already has. */
    if (!hk_butterworth4_highpass(&out->woofer_hpf, profile->woofer_hpf_hz, fs_hz)) {
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
    out->tweeter_sign = (profile->tweeter_polarity != 0u) ? -1.0f : 1.0f;
    out->woofer_delay_samples = profile->woofer_delay_samples;
    out->tweeter_delay_samples = profile->tweeter_delay_samples;

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
        .release_ms  = profile->woofer_release_ms,
        .hold_ms     = profile->woofer_hold_ms,
        .sample_rate = rate,
    };
    out->tweeter_limit = (hk_limiter_config_t){
        .ceiling     = tweeter_ceiling,
        .release_ms  = profile->tweeter_release_ms,
        .hold_ms     = profile->tweeter_hold_ms,
        .sample_rate = rate,
    };
    /* Stored as read: the budget was measured on the adapter and describes
     * the adapter, so the supply voltage this chain is built at does not
     * touch it. */
    out->supply_limit = (hk_supply_limiter_config_t){
        .budget_sq   = profile->supply_budget_sq,
        .window_ms   = profile->supply_window_ms,
        .sample_rate = rate,
    };

    /* Asked rather than assumed. The limiters have their own refusals and they
     * are allowed to be stricter than the ones above; a config this module
     * accepted and that module rejects would surface as a stage that never
     * engages. */
    if (!hk_limiter_config_valid(&out->woofer_limit) ||
        !hk_limiter_config_valid(&out->tweeter_limit) ||
        !hk_supply_limiter_config_valid(&out->supply_limit)) {
        return HK_PROFILE_UNBUILDABLE;
    }

    return HK_PROFILE_OK;
}

hk_profile_verdict_t hk_profile_build(const hk_profile_t *profile,
                                      float fs_hz, float supply_mv,
                                      hk_profile_chain_t *out)
{
    if (out == NULL) {
        return HK_PROFILE_UNBUILDABLE;
    }
    memset(out, 0, sizeof(*out));

    const hk_profile_verdict_t verdict = build_chain(profile, fs_hz, supply_mv, out);
    if (verdict != HK_PROFILE_OK) {
        /* A refusal part-way through would otherwise leave designed sections
         * next to zeroed ones -- a chain that looks half real. Nothing
         * half-built is left for a caller that ignores the verdict. */
        memset(out, 0, sizeof(*out));
    }
    return verdict;
}

hk_profile_verdict_t hk_profile_load(const void *blob, size_t length,
                                     float fs_hz, float supply_mv,
                                     hk_profile_t *profile_or_null,
                                     hk_profile_chain_t *chain_or_null)
{
    /* Built into locals and copied out only on OK, so the two outputs are
     * either both the result of one verdict or both zero -- never a valid
     * profile next to an empty chain. */
    hk_profile_t profile;
    hk_profile_chain_t chain;

    hk_profile_verdict_t verdict = hk_profile_from_blob(blob, length, &profile);
    if (verdict == HK_PROFILE_OK) {
        verdict = hk_profile_build(&profile, fs_hz, supply_mv, &chain);
    }

    if (verdict != HK_PROFILE_OK) {
        if (profile_or_null != NULL) {
            memset(profile_or_null, 0, sizeof(*profile_or_null));
        }
        if (chain_or_null != NULL) {
            memset(chain_or_null, 0, sizeof(*chain_or_null));
        }
        return verdict;
    }

    if (profile_or_null != NULL) {
        *profile_or_null = profile;
    }
    if (chain_or_null != NULL) {
        *chain_or_null = chain;
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
    case HK_PROFILE_BAD_DELAY:       return "delay";
    case HK_PROFILE_BAD_POLARITY:    return "polarity";
    case HK_PROFILE_BAD_BUDGET:      return "budget";
    case HK_PROFILE_BAD_AMP_GAIN:    return "amp-gain";
    default:                         return "unknown";
    }
}
