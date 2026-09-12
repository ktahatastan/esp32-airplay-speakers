/**
 * @file hk_profile.h
 * @brief The calibration profile: the shape of the numbers G0 and G2 will produce.
 *
 * The filters and the limiter are written and tested (hk_biquad, hk_limiter),
 * and both refuse to invent a frequency or a ceiling. This module is what will
 * hand them the real ones: a versioned record stored in `factory_cal`, and the
 * one function that turns it into a configured chain.
 *
 * NOTHING HERE CONTAINS A DRIVER VALUE, and it cannot: the woofer's and the
 * tweeter's impedance curves and `Fs` have not been measured (`G0`; only the
 * two DC resistances have), so any number written here today would be
 * indistinguishable from a measured one tomorrow. What is defined is
 * the FORM -- which fields exist, which combinations are refused, and how a
 * ceiling measured at one supply voltage becomes a ceiling at another. When the
 * measurements land, the work is filling a struct in, not designing one.
 *
 * Why the profile carries its own provenance
 * ------------------------------------------
 * The measured impedances are stored even though the runtime never computes
 * with them. Deriving a crossover is a bench step done by a person; the device
 * only carries the result. Recording what the result was derived FROM is what
 * makes a profile traceable to a measurement -- and a profile that cannot name
 * its measurement is exactly the guess this project refuses to run on.
 *
 * Why a ceiling needs a voltage attached to it
 * --------------------------------------------
 * A limiter ceiling is a digital number, and what reaches the driver is volts.
 * For a class-D amplifier at a fixed digital level, those volts follow the
 * supply -- and the supply here is whatever DC adapter is plugged into the
 * barrel jack: 24 V nominal (ADR-0020), feeding four XH-A232 in parallel,
 * while each XH-A232 accepts anything from 8 V to 26 V. So a single stored
 * ceiling protects the driver at exactly one supply voltage and is either
 * unsafe or needlessly quiet at every other -- and that is the product case,
 * not a hypothetical: the bench profile was listened to at 12 V, and replayed
 * unscaled on the 24 V adapter it would be too loud by half. The profile
 * stores the ceiling WITH the supply voltage it was measured at, and
 * hk_profile_ceiling_at() moves it to the configured one.
 *
 * The ceiling a profile stores is bounded twice on the bench: by G1's
 * supply-current budget (2.9 A with all four amplifiers driven) and by G2's
 * measured driver behaviour, whichever is lower.
 */
#ifndef HK_PROFILE_H
#define HK_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hk_biquad.h"
#include "hk_limiter.h"

/** Bump when a field changes meaning. hk_schema decides what an old one does. */
#define HK_PROFILE_SCHEMA 1u

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
} hk_profile_verdict_t;

/**
 * One speaker's calibration.
 *
 * Stored as a blob, so the field order is a wire format: append at the end and
 * bump ::HK_PROFILE_SCHEMA rather than reordering.
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
    float woofer_hpf_hz;   /**< Subsonic filter: excursion the woofer cannot make */
    float crossover_hz;    /**< LR4 corner: low branch to woofer, high to tweeter */
    float woofer_gain;     /**< Linear, (0, 1] */
    float tweeter_gain;    /**< Linear, (0, 1]. Level-matches the two branches */

    /* ---- Protection (G2), each ceiling tied to the voltage it was measured at ---- */
    float    reference_supply_mv;
    float    woofer_ceiling;   /**< At reference_supply_mv, linear full scale */
    float    tweeter_ceiling;  /**< At reference_supply_mv, linear full scale */
    uint32_t release_ms;
    uint32_t hold_ms;
} hk_profile_t;

/** Everything the audio task needs, built from a profile and the present state. */
typedef struct {
    hk_biquad_coeffs_t  woofer_hpf;   /**< Second order; the subsonic filter */
    hk_lr4_coeffs_t     woofer_low;   /**< LR4 low branch */
    hk_lr4_coeffs_t     tweeter_high; /**< LR4 high branch. In phase: do not invert */
    float               woofer_gain;
    float               tweeter_gain;
    hk_limiter_config_t woofer_limit;
    hk_limiter_config_t tweeter_limit;
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
 * The ceiling that protects the driver at the supply voltage the amplifier is
 * running from.
 *
 * What reaches the driver is (digital level x supply), so holding the volts
 * constant means moving the level the other way. A supply below the reference
 * allows a HIGHER digital ceiling for the same volts, capped at full scale --
 * past that there is no more signal to give. A supply above it, which is the
 * case when a bench profile meets the 24 V adapter, lowers the ceiling.
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

/** A one-word reason, for a log line or a bench report. */
const char *hk_profile_verdict_name(hk_profile_verdict_t verdict);

#endif /* HK_PROFILE_H */
