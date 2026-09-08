/**
 * @file hk_profile.h
 * @brief The calibration profile: the shape of the numbers G0 and G2 will produce.
 *
 * The filters and the limiter are written and tested (hk_biquad, hk_limiter),
 * and both refuse to invent a frequency or a ceiling. This module is what will
 * hand them the real ones: a versioned record stored in `factory_cal`, and the
 * one function that turns it into a configured chain.
 *
 * NOTHING HERE CONTAINS A DRIVER VALUE, and it cannot: the woofer and tweeter
 * impedances have not been measured (`G0`), so any number written here today
 * would be indistinguishable from a measured one tomorrow. What is defined is
 * the FORM -- which fields exist, which combinations are refused, and how a
 * ceiling measured at one pack voltage becomes a ceiling at another. When the
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
 * supply -- and the supply here is a battery that falls from 16.8 V to 12.0 V
 * as it empties (ADR-0003). So a single stored ceiling protects the driver at
 * exactly one state of charge and is either unsafe or needlessly quiet at
 * every other. The profile stores the ceiling WITH the pack voltage it was
 * measured at, and hk_profile_ceiling_at() moves it to the present one.
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
 * The 4S pack's working range, in millivolts (ADR-0003).
 *
 * A bound on what a stored REFERENCE voltage may claim, not a threshold the
 * device acts on -- those live in hk_power. A profile whose ceiling was
 * measured at a voltage this pack cannot reach describes some other speaker.
 */
#define HK_PROFILE_PACK_MV_MIN 10000.0f
#define HK_PROFILE_PACK_MV_MAX 17500.0f

/** What a validation refused, so a bad profile is diagnosable at the bench. */
typedef enum {
    HK_PROFILE_OK = 0,
    HK_PROFILE_BAD_SCHEMA,      /**< Not a profile this firmware understands */
    HK_PROFILE_NO_SOURCE,       /**< No measurement is named; not traceable */
    HK_PROFILE_BAD_MEASUREMENT, /**< A driver impedance is absent or impossible */
    HK_PROFILE_BAD_FREQUENCY,   /**< A corner is absent, impossible, or misordered */
    HK_PROFILE_BAD_GAIN,        /**< A branch gain is outside (0, 1] */
    HK_PROFILE_BAD_CEILING,     /**< A limiter ceiling is outside (0, 1] */
    HK_PROFILE_BAD_REFERENCE,   /**< The reference pack voltage is not this pack */
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
    float    reference_pack_mv;
    float    woofer_ceiling;   /**< At reference_pack_mv, linear full scale */
    float    tweeter_ceiling;  /**< At reference_pack_mv, linear full scale */
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
 * The ceiling that protects the driver at the pack voltage it has right now.
 *
 * What reaches the driver is (digital level x supply), so holding the volts
 * constant means moving the level the other way. A pack that has fallen below
 * the reference allows a HIGHER digital ceiling for the same volts, capped at
 * full scale -- past that there is no more signal to give.
 *
 * Returns 0 when it cannot answer, which every caller must treat as "do not
 * play" rather than as silence.
 */
float hk_profile_ceiling_at(float ceiling_at_reference, float reference_mv, float pack_mv);

/**
 * Turn a profile into a chain, at this sample rate and this pack voltage.
 *
 * Fails rather than clamping. A corner that the filter design refuses at this
 * sample rate is a profile written for a different one, and quietly moving it
 * would produce a crossover nobody chose.
 */
hk_profile_verdict_t hk_profile_build(const hk_profile_t *profile,
                                      float fs_hz, float pack_mv,
                                      hk_profile_chain_t *out);

/** A one-word reason, for a log line or a bench report. */
const char *hk_profile_verdict_name(hk_profile_verdict_t verdict);

#endif /* HK_PROFILE_H */
