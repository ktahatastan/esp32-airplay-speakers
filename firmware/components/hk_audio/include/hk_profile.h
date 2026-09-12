/**
 * @file hk_profile.h
 * @brief The calibration profile: the shape of the numbers G0, G1 and G2 will
 *        produce.
 *
 * The filters and the limiters are written and tested (hk_biquad, hk_limiter,
 * hk_supply_limiter), and all of them refuse to invent a frequency, a ceiling
 * or a budget. This module is what will hand them the real ones: a versioned
 * record stored in `factory_cal`, and the one function that turns it into a
 * configured chain.
 *
 * NOTHING HERE CONTAINS A DRIVER VALUE, and it cannot: the woofer's and the
 * tweeter's impedance curves and `Fs` have not been measured (`G0`; only the
 * two DC resistances have), so any number written here today would be
 * indistinguishable from a measured one tomorrow. What is defined is
 * the FORM -- which fields exist, which combinations are refused, and how a
 * ceiling measured at one supply voltage is carried to another. When the
 * measurements land, the work is filling a struct in, not designing one.
 *
 * Why the profile carries its own provenance
 * ------------------------------------------
 * The measured impedances are stored even though the runtime never computes
 * with them. Deriving a crossover is a bench step done by a person; the device
 * only carries the result. Recording what the result was derived FROM is what
 * makes a profile traceable to a measurement -- and a profile that cannot name
 * its measurement is exactly the guess this project refuses to run on. The
 * amplifier's gain setting (`amp_gain_db`) is carried for the same reason and
 * on the same terms: read off the board's straps in bench item C3, never
 * computed with here, and zero until it has been read.
 *
 * Why a ceiling carries a voltage, and what that does NOT promise
 * ---------------------------------------------------------------
 * A limiter ceiling is a digital number, and what reaches the driver is volts.
 * The profile stores each peak ceiling WITH the supply voltage the amplifier
 * ran from when the ceiling was chosen, and hk_profile_ceiling_at() scales it
 * DOWN by reference/supply when the configured supply is higher -- the
 * product case, where a profile listened to on the 12 V bench supply meets
 * the 24 V adapter (ADR-0020) and its ceilings halve.
 *
 * That scale-down errs quiet in the case it was written for, which is why it
 * is kept. It is NOT a guarantee that the volts at the driver stay what the
 * bench heard, and the record used to say it was. The TPA3110D2 is a
 * fixed-gain amplifier: its output is gain x input until the rail clips, not
 * a fraction of the rail. Its datasheet (SLOS528F, Table 3) shows the same
 * 1 Vrms input at 20 dB gain producing 23.5 Vpp from a 12 V rail and
 * 27.7 Vpp from 24 V -- the 12 V figure is the rail clipping, not the gain
 * halving. Two things follow. Where the bench level did NOT clip the 12 V
 * rail, the halved ceiling on the adapter is half the bench voltage: quieter,
 * not the same. Where the bench level DID clip that rail -- which, at an
 * unread gain strapping (bench item C3), a ceiling stated in full-scale
 * units may well have -- the 24 V rail clips later, and the halved ceiling
 * can still put more on the driver than the bench's clipped level. Which case
 * applies is exactly what C3 and the rail's clip point decide, and what a
 * ceiling really protects against is G2's driver limit; until both are read,
 * a profile built for the adapter is played staged, at low level, on one
 * pair.
 *
 * The scaling is ONE-DIRECTIONAL: a supply BELOW the reference never raises a
 * ceiling above its stored value. For a fixed-gain amplifier a lower rail does
 * not license a higher digital level -- it only moves the clip point down --
 * and the raise would be the one unsafe direction of the old rule.
 *
 * Why the current budget is a separate field
 * ------------------------------------------
 * The adapter's 2.9 A (ADR-0020) is an AVERAGE constraint on the SUM of both
 * branches, and a peak ceiling per branch cannot express either half of that
 * (hk_supply_limiter.h says why). So the budget has its own fields,
 * `supply_budget_sq` and `supply_window_ms`, measured on the adapter in G1
 * step S7 with all four amplifiers into 4 ohm-class loads, and stored as read:
 * it describes the adapter, so it is never scaled by supply voltage.
 */
#ifndef HK_PROFILE_H
#define HK_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hk_biquad.h"
#include "hk_limiter.h"
#include "hk_supply_limiter.h"

/**
 * Bump when a field changes meaning. The factory store's own version
 * (hk_schema) is unchanged by this -- its layout did not move -- so an old
 * profile blob is refused here, by length in hk_profile_from_blob() and by
 * schema in hk_profile_valid(), and no converter exists because none was ever
 * written to a device.
 *
 * 2: per-branch limiter timing, one-branch alignment delay, tweeter polarity,
 *    the supply budget, and the amplifier gain as provenance; and the subsonic
 *    filter became fourth order, so `woofer_hpf_hz` now names the -3 dB corner
 *    of a Butterworth-4 rather than of a single section (same point, steeper
 *    skirt). A schema-1 blob (84 bytes) is refused by name.
 */
#define HK_PROFILE_SCHEMA 2u

/** How long a measurement reference may be, including its terminator. */
#define HK_PROFILE_SOURCE_MAX 32

/**
 * The amplifier's rated input range, in millivolts (XH-A232: 8-26 V).
 *
 * A bound on what a stored REFERENCE voltage may claim, not a threshold the
 * device acts on. A profile whose ceiling was measured at a voltage this
 * amplifier cannot be fed describes some other speaker.
 */
#define HK_PROFILE_SUPPLY_MV_MIN 8000.0f
#define HK_PROFILE_SUPPLY_MV_MAX 26000.0f

/**
 * Longest alignment delay a branch may carry: 1.45 ms at 44.1 kHz.
 *
 * An acoustic-centre correction between a 60 mm cone and a 25 mm dome in the
 * same baffle is a fraction of a millisecond; this is several times that, and
 * still well inside the +/-2.9 ms jitter the timing report already absorbs.
 * It is a bound on a correction, not a lookahead.
 */
#define HK_PROFILE_DELAY_MAX_SAMPLES 64u

/** Largest supply budget a profile may claim: two full-scale branches. */
#define HK_PROFILE_SUPPLY_BUDGET_MAX HK_SUPPLY_LIMITER_BUDGET_MAX

/** What a validation refused, so a bad profile is diagnosable at the bench. */
typedef enum {
    HK_PROFILE_OK = 0,
    HK_PROFILE_BAD_SCHEMA,      /**< Not a profile this firmware understands */
    HK_PROFILE_NO_SOURCE,       /**< No measurement is named; not traceable */
    HK_PROFILE_BAD_MEASUREMENT, /**< A driver impedance is absent or impossible */
    HK_PROFILE_BAD_FREQUENCY,   /**< A corner is absent, impossible, or misordered */
    HK_PROFILE_BAD_GAIN,        /**< A branch gain is outside (0, 1] */
    HK_PROFILE_BAD_CEILING,     /**< A limiter ceiling is outside (0, 1] */
    HK_PROFILE_BAD_REFERENCE,   /**< The reference supply voltage is outside what the amplifier can be fed */
    HK_PROFILE_BAD_TIMING,      /**< A release time of zero is a switch, not a release */
    HK_PROFILE_UNBUILDABLE,     /**< Valid on its own, impossible at this sample rate */
    HK_PROFILE_BAD_DELAY,       /**< A delay is over the bound, or both branches are delayed */
    HK_PROFILE_BAD_POLARITY,    /**< Tweeter polarity is neither 0 nor 1 */
    HK_PROFILE_BAD_BUDGET,      /**< The supply budget or its window is absent or impossible */
    HK_PROFILE_BAD_AMP_GAIN,    /**< An amplifier gain that is not one of the TPA3110D2's four settings */
} hk_profile_verdict_t;

/**
 * One speaker's calibration.
 *
 * Stored as a blob, so the field order is a wire format: append at the end and
 * bump ::HK_PROFILE_SCHEMA rather than reordering. Every field is four bytes
 * wide or a multiple of four, so the struct has no padding and its size is the
 * wire length; test_profile asserts the size as a literal so a reorder or an
 * unintended padding byte is caught on the host.
 */
typedef struct {
    uint16_t schema;              /**< ::HK_PROFILE_SCHEMA when written */
    uint16_t reserved;            /**< Zero. Keeps the float alignment explicit */
    uint32_t measured_yyyymmdd;   /**< The day the bench produced these numbers */
    char     source[HK_PROFILE_SOURCE_MAX]; /**< The measurement record's own name */

    /* ---- What was measured (G0). Carried, never computed with. ---- */
    float woofer_dcr_ohm;
    float tweeter_dcr_ohm;

    /* ---- What was derived from it, on a bench, by a person. ---- */
    float woofer_hpf_hz;   /**< Subsonic filter, Butterworth-4: its -3 dB corner. Excursion the woofer cannot make */
    float crossover_hz;    /**< LR4 corner: low branch to woofer, high to tweeter */
    float woofer_gain;     /**< Linear, (0, 1] */
    float tweeter_gain;    /**< Linear, (0, 1]. Level-matches the two branches */

    /* ---- Protection (G2), each ceiling tied to the voltage it was measured at ---- */
    float    reference_supply_mv;
    float    woofer_ceiling;   /**< At reference_supply_mv, linear full scale */
    float    tweeter_ceiling;  /**< At reference_supply_mv, linear full scale */
    uint32_t woofer_release_ms;   /**< Woofer peak limiter recovery (G2) */
    uint32_t woofer_hold_ms;

    /* ---- Schema 2 ---- */
    uint32_t tweeter_release_ms;  /**< Tweeter peak limiter recovery (G2); a dome and a cone do not recover alike */
    uint32_t tweeter_hold_ms;
    uint32_t woofer_delay_samples;  /**< Acoustic-centre alignment (G2 step 5). At most one branch is delayed */
    uint32_t tweeter_delay_samples; /**< <= ::HK_PROFILE_DELAY_MAX_SAMPLES */
    uint32_t tweeter_polarity;      /**< 0 in phase, 1 inverted. Decided by the G2 sum measurement, not by LR4 theory */
    float    supply_budget_sq;      /**< Mean of (woofer^2 + tweeter^2) the adapter allows (G1 S7). Stored as read, never scaled */
    uint32_t supply_window_ms;      /**< The averaging window that budget was measured with (G1 S7) */
    uint32_t amp_gain_db;           /**< 0 = not read; else 20, 26, 32 or 36 -- the TPA3110D2's four strap settings (bench item C3). Carried, never computed with */
} hk_profile_t;

/** Everything the audio task needs, built from a profile and the present state. */
typedef struct {
    hk_lr4_coeffs_t     woofer_hpf;   /**< Fourth-order Butterworth; the subsonic filter */
    hk_lr4_coeffs_t     woofer_low;   /**< LR4 low branch */
    hk_lr4_coeffs_t     tweeter_high; /**< LR4 high branch. In phase with the low branch */
    float               woofer_gain;
    float               tweeter_gain;
    float               tweeter_sign;         /**< +1 or -1, from tweeter_polarity */
    uint32_t            woofer_delay_samples; /**< At most one of these is non-zero */
    uint32_t            tweeter_delay_samples;
    hk_limiter_config_t woofer_limit;
    hk_limiter_config_t tweeter_limit;
    hk_supply_limiter_config_t supply_limit;  /**< The adapter budget, unscaled */
} hk_profile_chain_t;

/**
 * Judge a profile on its own, without a sample rate.
 *
 * Structural only: it cannot tell a correct crossover from a wrong one, because
 * only a measurement can. What it does catch is a profile that is not a
 * profile -- absent numbers, impossible ones, and orderings that contradict
 * themselves.
 */
hk_profile_verdict_t hk_profile_valid(const hk_profile_t *profile);

/**
 * Read a profile out of stored bytes.
 *
 * The size is checked before the schema, because a blob of the wrong length is
 * not a profile of another version -- it is not a profile.
 */
hk_profile_verdict_t hk_profile_from_blob(const void *blob, size_t length,
                                          hk_profile_t *out);

/**
 * The ceiling to run with at the supply voltage the amplifier is fed from.
 *
 * ceiling x min(1, reference / supply). A supply above the reference -- a
 * bench profile meeting the 24 V adapter -- lowers the ceiling by the ratio,
 * which errs quiet. A supply below it returns the stored ceiling UNCHANGED:
 * the amplifier's gain is fixed, so a lower rail moves the clip point, not
 * the volts a given digital level produces, and there is no headroom to give
 * back (see the file header for the datasheet reading behind this).
 *
 * Returns 0 when it cannot answer, which every caller must treat as "do not
 * play" rather than as silence.
 */
float hk_profile_ceiling_at(float ceiling_at_reference, float reference_mv, float supply_mv);

/**
 * Turn a profile into a chain, at this sample rate and this supply voltage.
 *
 * Fails rather than clamping. A corner that the filter design refuses at this
 * sample rate is a profile written for a different one, and quietly moving it
 * would produce a crossover nobody chose.
 */
hk_profile_verdict_t hk_profile_build(const hk_profile_t *profile,
                                      float fs_hz, float supply_mv,
                                      hk_profile_chain_t *out);

/**
 * The one judge: stored bytes in, verdict out, and optionally the profile and
 * the chain that verdict was reached on.
 *
 * hk_profile_from_blob() then hk_profile_build(), in one call, so that the
 * boot gate in hk_main and the output backend cannot disagree about a blob:
 * both ask this function and get the same answer. Either output may be NULL
 * when only the verdict is wanted. On any verdict but ::HK_PROFILE_OK both
 * outputs are zeroed -- nothing half-read or half-built is left for a caller
 * that ignores the return.
 */
hk_profile_verdict_t hk_profile_load(const void *blob, size_t length,
                                     float fs_hz, float supply_mv,
                                     hk_profile_t *profile_or_null,
                                     hk_profile_chain_t *chain_or_null);

/** A one-word reason, for a log line or a bench report. */
const char *hk_profile_verdict_name(hk_profile_verdict_t verdict);

#endif /* HK_PROFILE_H */
