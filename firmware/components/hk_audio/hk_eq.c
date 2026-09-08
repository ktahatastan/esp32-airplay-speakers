#include "hk_eq.h"

#include <math.h>
#include <string.h>

/*
 * The rows. Every default is the FLAT one: gain at ::HK_EQ_GAIN_ZERO_U32 and
 * trim at zero. The frequencies have defaults too, and those are placements
 * rather than voicings -- a band sitting at 100 Hz with 0 dB of gain does
 * nothing at all, so choosing where it starts costs nothing and saves the
 * owner from tuning a control that begins at 20 Hz.
 *
 * Key length is capped at 15 by NVS (HK_SETTINGS_KEY_MAX); the longest here is
 * "eq_mid_hz" at 9.
 */
const hk_setting_def_t hk_eq_settings_table[] = {
    {"eq_lo_hz",  "EQ low shelf corner, Hz",        100u,  HK_EQ_HZ_MIN_U32, HK_EQ_HZ_MAX_U32},
    {"eq_lo_db",  "EQ low shelf gain, 0.1 dB + 120", HK_EQ_GAIN_ZERO_U32, 0u, HK_EQ_GAIN_MAX_U32},
    {"eq_lo_q",   "EQ low shelf slope, Q x 100",     71u,  HK_EQ_Q_MIN_U32,  HK_EQ_Q_MAX_U32},

    {"eq_mid_hz", "EQ mid centre, Hz",             1000u,  HK_EQ_HZ_MIN_U32, HK_EQ_HZ_MAX_U32},
    {"eq_mid_db", "EQ mid gain, 0.1 dB + 120",      HK_EQ_GAIN_ZERO_U32, 0u, HK_EQ_GAIN_MAX_U32},
    {"eq_mid_q",  "EQ mid width, Q x 100",          100u, HK_EQ_Q_MIN_U32,  HK_EQ_Q_MAX_U32},

    {"eq_hi_hz",  "EQ high shelf corner, Hz",      8000u,  HK_EQ_HZ_MIN_U32, HK_EQ_HZ_MAX_U32},
    {"eq_hi_db",  "EQ high shelf gain, 0.1 dB + 120", HK_EQ_GAIN_ZERO_U32, 0u, HK_EQ_GAIN_MAX_U32},
    {"eq_hi_q",   "EQ high shelf slope, Q x 100",   71u,  HK_EQ_Q_MIN_U32,  HK_EQ_Q_MAX_U32},

    {"eq_trim",   "master cut, 0.1 dB (0 = none)",   0u,  0u, HK_EQ_TRIM_MAX_U32},
    {NULL, NULL, 0u, 0u, 0u},
};

/** Which row of the table each band's three fields start at. */
#define ROW_BAND_STRIDE 3u

size_t hk_eq_settings_count(void)
{
    size_t n = 0;
    while (hk_eq_settings_table[n].key != NULL) {
        n++;
    }
    return n;
}

/* ---- Encoding. The u32 table is unsigned, and a tone control must cut. ---- */

static float gain_db_from_u32(uint32_t stored)
{
    return ((float)stored - (float)HK_EQ_GAIN_ZERO_U32) * 0.1f;
}

static float q_from_u32(uint32_t stored)
{
    return (float)stored * 0.01f;
}

static float trim_db_from_u32(uint32_t stored)
{
    /* Stored is a CUT in tenths of a dB, so the sign is applied here rather
     * than stored. Zero means no cut, which is what a fresh device runs. */
    return -((float)stored * 0.1f);
}

hk_eq_settings_t hk_eq_defaults(void)
{
    hk_eq_settings_t out;
    memset(&out, 0, sizeof(out));

    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        const size_t row = b * ROW_BAND_STRIDE;
        out.band[b].hz      = (float)hk_eq_settings_table[row + 0u].fallback;
        out.band[b].gain_db = gain_db_from_u32(hk_eq_settings_table[row + 1u].fallback);
        out.band[b].q       = q_from_u32(hk_eq_settings_table[row + 2u].fallback);
    }
    out.trim_db = trim_db_from_u32(hk_eq_settings_table[HK_EQ_BANDS * ROW_BAND_STRIDE].fallback);
    return out;
}

bool hk_eq_settings_load(hk_eq_settings_t *out, hk_eq_read_u32_fn read, void *ctx)
{
    if (out == NULL) {
        return false;
    }
    *out = hk_eq_defaults();
    if (read == NULL) {
        return false;
    }

    bool all_stored = true;
    uint32_t decoded[HK_EQ_BANDS * ROW_BAND_STRIDE + 1u];

    for (size_t i = 0; i < (HK_EQ_BANDS * ROW_BAND_STRIDE + 1u); i++) {
        const hk_setting_def_t *def = &hk_eq_settings_table[i];
        uint32_t stored = 0u;
        const bool present = read(def->key, &stored, ctx);
        hk_setting_origin_t origin = HK_SETTING_DEFAULTED;

        /* Reused rather than reimplemented: a value outside its range must
         * fall back to the default and SAY so, never be clamped into looking
         * like something the owner chose. That policy is hk_settings' and
         * belongs in one place. */
        decoded[i] = hk_settings_resolve(def, stored, present, &origin);
        if (origin != HK_SETTING_STORED) {
            all_stored = false;
        }
    }

    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        const size_t row = b * ROW_BAND_STRIDE;
        out->band[b].hz      = (float)decoded[row + 0u];
        out->band[b].gain_db = gain_db_from_u32(decoded[row + 1u]);
        out->band[b].q       = q_from_u32(decoded[row + 2u]);
    }
    out->trim_db = trim_db_from_u32(decoded[HK_EQ_BANDS * ROW_BAND_STRIDE]);

    return all_stored;
}

/* ---- Design. RBJ Audio EQ Cookbook, normalised the way hk_biquad is. ---- */

bool hk_eq_design(hk_biquad_coeffs_t *coeffs, hk_eq_band_kind_t kind,
                  float fc_hz, float fs_hz, float q, float gain_db)
{
    if (coeffs == NULL) {
        return false;
    }
    if (!isfinite(fc_hz) || !isfinite(fs_hz) || !isfinite(q) || !isfinite(gain_db)) {
        return false;
    }
    if (!(fs_hz > 0.0f) || !(q > 0.0f) || !(fc_hz > 0.0f)) {
        return false;
    }
    /* The same two frequency limits hk_biquad enforces, for the same reasons:
     * above Nyquist there is no filter to design, and below this fraction of
     * the sample rate the coefficients stop describing a stable one in single
     * precision. Repeated rather than shared because hk_biquad's checker is
     * static to that file; the constant they both use is not. */
    if (fc_hz >= fs_hz / 2.0f || fc_hz < fs_hz * HK_BIQUAD_MIN_FC_RATIO) {
        return false;
    }
    if (fabsf(gain_db) > HK_EQ_GAIN_DB_MAX) {
        return false;
    }

    const float a_gain = powf(10.0f, gain_db / 40.0f);
    const float w0 = 2.0f * (float)M_PI * fc_hz / fs_hz;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float sqrt_a = sqrtf(a_gain);
    const float two_sqrt_a_alpha = 2.0f * sqrt_a * alpha;

    float b0, b1, b2, a0, a1, a2;

    switch (kind) {
    case HK_EQ_PEAKING:
        b0 = 1.0f + alpha * a_gain;
        b1 = -2.0f * cos_w0;
        b2 = 1.0f - alpha * a_gain;
        a0 = 1.0f + alpha / a_gain;
        a1 = -2.0f * cos_w0;
        a2 = 1.0f - alpha / a_gain;
        break;

    case HK_EQ_LOW_SHELF:
        b0 = a_gain * ((a_gain + 1.0f) - (a_gain - 1.0f) * cos_w0 + two_sqrt_a_alpha);
        b1 = 2.0f * a_gain * ((a_gain - 1.0f) - (a_gain + 1.0f) * cos_w0);
        b2 = a_gain * ((a_gain + 1.0f) - (a_gain - 1.0f) * cos_w0 - two_sqrt_a_alpha);
        a0 = (a_gain + 1.0f) + (a_gain - 1.0f) * cos_w0 + two_sqrt_a_alpha;
        a1 = -2.0f * ((a_gain - 1.0f) + (a_gain + 1.0f) * cos_w0);
        a2 = (a_gain + 1.0f) + (a_gain - 1.0f) * cos_w0 - two_sqrt_a_alpha;
        break;

    case HK_EQ_HIGH_SHELF:
        b0 = a_gain * ((a_gain + 1.0f) + (a_gain - 1.0f) * cos_w0 + two_sqrt_a_alpha);
        b1 = -2.0f * a_gain * ((a_gain - 1.0f) + (a_gain + 1.0f) * cos_w0);
        b2 = a_gain * ((a_gain + 1.0f) + (a_gain - 1.0f) * cos_w0 - two_sqrt_a_alpha);
        a0 = (a_gain + 1.0f) - (a_gain - 1.0f) * cos_w0 + two_sqrt_a_alpha;
        a1 = 2.0f * ((a_gain - 1.0f) - (a_gain + 1.0f) * cos_w0);
        a2 = (a_gain + 1.0f) - (a_gain - 1.0f) * cos_w0 - two_sqrt_a_alpha;
        break;

    default:
        return false;
    }

    if (!isfinite(a0) || a0 == 0.0f) {
        return false;
    }

    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = a1 / a0;
    coeffs->a2 = a2 / a0;

    /* The last word belongs to hk_biquad, not to the algebra above. These
     * coefficients are derived from user settings, which is exactly the case
     * hk_biquad_stable() documents itself as existing for. */
    return hk_biquad_stable(coeffs);
}

/** Which kind of band sits at each position. Fixed by design, see hk_eq.h. */
static hk_eq_band_kind_t kind_of(size_t index)
{
    switch (index) {
    case 0:  return HK_EQ_LOW_SHELF;
    case 1:  return HK_EQ_PEAKING;
    default: return HK_EQ_HIGH_SHELF;
    }
}

bool hk_eq_build(hk_eq_t *eq, const hk_eq_settings_t *settings, float fs_hz)
{
    if (eq == NULL) {
        return false;
    }
    memset(eq, 0, sizeof(*eq));
    eq->trim = 1.0f;

    if (settings == NULL || !isfinite(fs_hz) || !(fs_hz > 0.0f)) {
        return false;
    }

    /* A trim outside its range is not a preference either. Refusing to apply
     * it leaves the equaliser at unity, which is the safe direction: an
     * unapplied CUT can only make the signal louder than intended by the
     * amount of the cut, and the limiter is downstream of it. */
    if (isfinite(settings->trim_db) && settings->trim_db <= 0.0f &&
        settings->trim_db >= -HK_EQ_TRIM_DB_MAX) {
        eq->trim = powf(10.0f, settings->trim_db / 20.0f);
        if (!isfinite(eq->trim) || !(eq->trim > 0.0f) || eq->trim > 1.0f) {
            eq->trim = 1.0f;
        }
    }

    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        const hk_eq_band_t *band = &settings->band[b];

        /* Exactly flat is not designed. A 0 dB peaking section is arithmetic
         * that returns almost its input -- almost, because b and a differ in
         * the last bits -- so skipping it makes the default chain bit-exact
         * AND free, instead of three biquads that nearly cancel. */
        if (isfinite(band->gain_db) && band->gain_db == 0.0f) {
            continue;
        }

        if (hk_eq_design(&eq->stage[b].coeffs, kind_of(b), band->hz, fs_hz,
                         band->q, band->gain_db)) {
            eq->stage[b].active = true;
        } else {
            /* Recorded, not fatal. A tone control that will not design is a
             * control that does nothing; it is not a reason to stop playing. */
            eq->rejected |= (uint8_t)(1u << b);
        }
    }

    return true;
}

void hk_eq_reset(hk_eq_t *eq)
{
    if (eq == NULL) {
        return;
    }
    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        eq->stage[b].state.z1 = 0.0f;
        eq->stage[b].state.z2 = 0.0f;
    }
}

float hk_eq_process_one(hk_eq_t *eq, float sample)
{
    if (eq == NULL) {
        return sample;
    }
    float y = sample;
    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        if (eq->stage[b].active) {
            y = hk_biquad_process_one(&eq->stage[b].coeffs, &eq->stage[b].state, y);
        }
    }
    return y * eq->trim;
}

size_t hk_eq_active_bands(const hk_eq_t *eq)
{
    if (eq == NULL) {
        return 0;
    }
    size_t n = 0;
    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        if (eq->stage[b].active) {
            n++;
        }
    }
    return n;
}
