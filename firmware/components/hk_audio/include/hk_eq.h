/**
 * @file hk_eq.h
 * @brief The tonal half of the chain: what the owner is allowed to change.
 *
 * There are two kinds of number in this speaker's signal path and they are not
 * the same kind of thing, so they do not live in the same place.
 *
 * PROTECTIVE numbers -- the subsonic corner, the crossover corner, the branch
 * gains, the limiter ceilings -- come off a bench, describe a driver, and a
 * wrong one destroys hardware. They live in ::hk_profile_t, in the read-only
 * `factory_cal` partition, and a user reset cannot reach them (PRD-008).
 *
 * TONAL numbers -- everything in this file -- are a preference. A wrong one
 * sounds bad. They live in the ordinary user settings namespace, they are
 * erased by a reset, and they are expected to change often, because the owner
 * will tune this speaker by ear in the room it stands in.
 *
 * NOTHING HERE HAS A VOICING BY DEFAULT, and that is the same refusal
 * hk_profile.h makes for a different reason. A default treble lift would be a
 * claim about how these drivers sound, and no acoustic measurement exists --
 * no measurement microphone, no near-field response, nothing (see
 * `docs/02-hardware/driver-measurements.md`). So every band's default gain is
 * exactly 0 dB, and a band at exactly 0 dB is not designed at all: it is
 * skipped, so the default chain is bit-exact passthrough costing zero
 * multiplies rather than three biquads that almost cancel.
 *
 * Why these three bands
 * ---------------------
 * hk_biquad offers low-pass and high-pass designs and nothing else, because
 * that is all a Linkwitz-Riley crossover needs. A tone control needs shelves
 * and a peak, so the DESIGN formulas (RBJ cookbook) are here -- but the
 * runtime is not reimplemented: these coefficients go into the same
 * ::hk_biquad_coeffs_t, through the same Direct Form II transposed
 * hk_biquad_process_one(), and past the same hk_biquad_stable() check that
 * exists precisely for "coefficients that arrive from [storage], where nothing
 * has guaranteed anything". User settings are exactly that category.
 *
 * Three bands, fixed in role and free in placement: a low shelf for the room's
 * bass, one peaking band for whatever the owner finds objectionable, a high
 * shelf for air. Two would not cover a midrange problem; five would be five
 * more ways to store a number that fights the crossover, and the owner tuning
 * by ear cannot use them.
 *
 * What a band cannot do
 * ---------------------
 * It cannot move a crossover corner, a subsonic corner or a limiter ceiling,
 * because none of those values is in this struct and hk_dsp never writes them.
 * See hk_dsp.h for how that is guaranteed rather than intended.
 */
#ifndef HK_EQ_H
#define HK_EQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hk_biquad.h"
#include "hk_settings.h"

/** Bands in the user equaliser. See the header comment for why three. */
#define HK_EQ_BANDS 3u

/** Widest cut or boost a band may ask for, in dB. */
#define HK_EQ_GAIN_DB_MAX 12.0f

/** Deepest master trim, in dB. Cut only; see ::hk_eq_settings_t::trim_db. */
#define HK_EQ_TRIM_DB_MAX 24.0f

/**
 * How a band's dB gain is encoded in the u32 settings table.
 *
 * hk_setting_def_t carries unsigned values only, and a tone control has to be
 * able to cut. So gain is stored as tenths of a dB with this offset:
 * stored 0 is -12.0 dB, stored 240 is +12.0 dB, and stored 120 is FLAT.
 */
#define HK_EQ_GAIN_ZERO_U32 120u
#define HK_EQ_GAIN_MAX_U32  240u

/** Q is stored in hundredths: 71 is 0.71, 100 is 1.00. */
#define HK_EQ_Q_MIN_U32 20u
#define HK_EQ_Q_MAX_U32 500u

/** Band frequency bounds, in Hz. Audible range; the design refuses the rest. */
#define HK_EQ_HZ_MIN_U32 20u
#define HK_EQ_HZ_MAX_U32 20000u

/** Master trim, in tenths of a dB of ATTENUATION. Stored 0 is no cut. */
#define HK_EQ_TRIM_MAX_U32 240u

/** What a band does to the spectrum. Fixed by position, not chosen. */
typedef enum {
    HK_EQ_LOW_SHELF = 0, /**< Everything below the corner, lifted or cut */
    HK_EQ_PEAKING,       /**< A bell centred on the corner */
    HK_EQ_HIGH_SHELF,    /**< Everything above the corner */
} hk_eq_band_kind_t;

/** One band's tonal intent, in engineering units. */
typedef struct {
    float hz;      /**< Corner or centre frequency */
    float gain_db; /**< Within +/- ::HK_EQ_GAIN_DB_MAX. Exactly 0 means bypass */
    float q;       /**< Shelf slope, or bell width */
} hk_eq_band_t;

/**
 * The whole tonal setting: three bands and one broadband trim.
 *
 * The trim CUTS ONLY, and that asymmetry is the point. Three bands boosted
 * together can put the signal well above the level the profile's ceilings
 * allow, at which point the limiter is working continuously and the speaker
 * sounds squashed rather than loud. The trim is what buys that headroom back.
 * A trim that could also boost would be a second volume control fighting the
 * first one, and it would create the problem it exists to solve.
 */
typedef struct {
    hk_eq_band_t band[HK_EQ_BANDS];
    float        trim_db; /**< <= 0. Broadband attenuation, applied with the EQ */
} hk_eq_settings_t;

/** One band's built state: coefficients, filter memory, and whether it runs. */
typedef struct {
    hk_biquad_coeffs_t coeffs;
    hk_biquad_state_t  state;
    bool               active;
} hk_eq_stage_t;

/** The built equaliser. Owned by the caller; the module keeps no globals. */
typedef struct {
    hk_eq_stage_t stage[HK_EQ_BANDS];
    float         trim;     /**< Linear, (0, 1]. ::hk_eq_settings_t::trim_db */
    uint8_t       rejected; /**< Bitmask of bands that would not design */
} hk_eq_t;

/**
 * The settings rows this module needs, in the project's own settings format.
 *
 * These are ::hk_setting_def_t rows and they obey every rule
 * hk_settings_table_check() enforces -- key length, uniqueness, a default
 * inside its own range. They are declared HERE rather than appended to
 * `hk_settings_table[]` for one reason: that table is another file with
 * another owner, and this project's contract is one writer per file. Splicing
 * these rows in is a one-line change for whoever owns hk_settings.c, and
 * test_dsp checks that no key in this table collides with one in that table,
 * so the splice is proven safe before it is made.
 *
 * NULL-terminated, same as `hk_settings_table[]`.
 */
extern const hk_setting_def_t hk_eq_settings_table[];

/** Number of rows, excluding the terminator. */
size_t hk_eq_settings_count(void);

/**
 * Flat.
 *
 * Every band at exactly 0 dB and no trim, which hk_eq_build() turns into an
 * equaliser with no active stage at all. This is what a device with no stored
 * settings runs, and it is what the owner gets back after a reset.
 */
hk_eq_settings_t hk_eq_defaults(void);

/** How a caller reads one stored u32. False means nothing was stored. */
typedef bool (*hk_eq_read_u32_fn)(const char *key, uint32_t *out, void *ctx);

/**
 * Decode stored settings into engineering units.
 *
 * Every row goes through hk_settings_resolve(), so a stored value outside its
 * range falls back to that row's default instead of being clamped -- a
 * corrupted byte must not look like a preference. A NULL @p read yields
 * hk_eq_defaults().
 *
 * @return true if every row came from storage; false if any row fell back.
 *         The answer is diagnostic only: the settings returned are usable
 *         either way, because tonal data is never a reason to stop playing.
 */
bool hk_eq_settings_load(hk_eq_settings_t *out, hk_eq_read_u32_fn read, void *ctx);

/**
 * Build the equaliser.
 *
 * A band that cannot be designed at this sample rate -- an impossible
 * frequency, a Q of zero, coefficients that come out unstable -- is BYPASSED
 * and recorded in ::hk_eq_t::rejected, not treated as a failure. That is the
 * deliberate opposite of hk_profile_build(), which refuses outright, and the
 * difference is what the two kinds of number are for: a protective value that
 * cannot be honoured means the drivers are unprotected and nothing may play,
 * while a tonal value that cannot be honoured means one tone control does
 * nothing. Taking a speaker off the air over a bad treble setting would be a
 * fault, not a safeguard.
 *
 * @return false only when @p out or @p settings is NULL, or the sample rate is
 *         not a sample rate. The equaliser is flat in that case, never
 *         uninitialised.
 */
bool hk_eq_build(hk_eq_t *eq, const hk_eq_settings_t *settings, float fs_hz);

/** Clear the filter memory without changing the coefficients. */
void hk_eq_reset(hk_eq_t *eq);

/** One sample through every active band and the trim. */
float hk_eq_process_one(hk_eq_t *eq, float sample);

/** How many biquads this equaliser actually runs. Zero when flat. */
size_t hk_eq_active_bands(const hk_eq_t *eq);

/**
 * Design one band directly.
 *
 * Exposed because the cookbook algebra is the part worth testing against a
 * known response, and driving it through a whole settings decode to reach it
 * would test the decode instead.
 *
 * @return false if the request cannot produce a stable section. The same
 *         frequency limits as hk_biquad apply, because it is the same runtime.
 */
bool hk_eq_design(hk_biquad_coeffs_t *coeffs, hk_eq_band_kind_t kind,
                  float fc_hz, float fs_hz, float q, float gain_db);

#endif /* HK_EQ_H */
