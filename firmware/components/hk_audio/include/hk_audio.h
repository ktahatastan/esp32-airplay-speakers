/**
 * @file hk_audio.h
 * @brief Turning sound on and off in an order that does not damage anything.
 *
 * Two things have to move — the I2S clocks and the DAC's soft-mute — and the
 * order matters in both directions. There is no third thing: the XH-A232
 * amplifier boards have no shutdown or mute input of any kind (power in, audio
 * in, speakers out), so they are live from the moment the 24 V rail is, and
 * the PCM5102A's XSMT is the only point in the chain where this firmware can
 * stop sound. Everything the amplifiers reproduce is whatever the DAC lets
 * through, multiplied by their gain, into a tweeter whose impedance this
 * project has not yet measured.
 *
 * Coming up, the clock has to be running and stable before the DAC unmutes,
 * because a DAC unmuted into an absent or settling bit clock puts a step on its
 * output — and with the amplifiers always live, that step arrives at the
 * drivers.
 *
 * Going down, the DAC is muted first and the clocks are held for it. XSMT is a
 * soft mute: the PCM5102A ramps its output down rather than cutting it, and it
 * needs its bit clock to do that. Stopping the clocks first would cut the ramp
 * short and hand the amplifiers exactly the transient the ramp exists to avoid.
 *
 * The state machine is pure and takes its timings as arguments, so an entire
 * start-up and shutdown can be driven in a test in microseconds instead of
 * seconds, and so the settle times can come from G1 measurements rather
 * than from a guess made here.
 *
 * The invariant worth stating once, because everything else follows from it:
 * MUTED IS THE RESTING STATE. Every path that loses permission, loses the
 * stream, or does not understand its inputs ends with the DAC muted, and it
 * gets there by muting the DAC before anything else moves.
 */
#ifndef HK_AUDIO_H
#define HK_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

/** Where the output chain is in its sequence. */
typedef enum {
    HK_AUDIO_SILENT = 0,  /**< Clocks off, DAC muted */
    HK_AUDIO_CLOCKING,    /**< Clocks running, DAC still muted; waiting to settle */
    HK_AUDIO_PLAYING,     /**< DAC unmuted */
    HK_AUDIO_MUTING,      /**< DAC muted first, clocks held for the ramp; unwinding to silence */
} hk_audio_state_t;

/** What the rest of the device says about whether sound is allowed and wanted. */
typedef struct {
    bool     permitted;   /**< A driver-protection profile is present (or a bench exception stands in) */
    bool     stream_live; /**< AirPlay is delivering audio */
    uint32_t now_ms;
} hk_audio_inputs_t;

/**
 * Settle times.
 *
 * No defaults, for the usual reason: the right values come from watching the
 * rails and the outputs on a scope at G1, and a number invented here would
 * be indistinguishable from a measured one. A test supplies its own.
 */
typedef struct {
    uint32_t clock_settle_ms; /**< Clocks running before the DAC may unmute */
    uint32_t mute_settle_ms;  /**< DAC muted before the clocks may stop */
} hk_audio_timing_t;

/** The two lines this module drives. XSMT is active low in hardware. */
typedef struct {
    bool i2s_running;   /**< Clocks and data are being produced */
    bool dac_unmuted;   /**< PCM5102A XSMT released */
} hk_audio_outputs_t;

/**
 * The sequencer's own state, owned by the caller.
 *
 * Same shape as hk_prov_t: a small struct the caller keeps and a step function
 * that advances it. No globals, so a test can run a hundred start-up and
 * shutdown cycles in a loop.
 */
typedef struct {
    hk_audio_state_t state;
    uint32_t         entered_ms; /**< When the current state began */
} hk_audio_t;

/** Start in the resting state: everything muted. */
void hk_audio_init(hk_audio_t *seq, uint32_t now_ms);

/**
 * Advance the sequence by one tick.
 *
 * Losing permission or losing the stream while the DAC is unmuted moves to
 * ::HK_AUDIO_MUTING rather than jumping straight to ::HK_AUDIO_SILENT, so the
 * DAC is muted first and the clocks stay up until its ramp is done.
 *
 * A NULL @p seq, @p inputs or @p timing takes the safe path rather than doing
 * nothing: not knowing whether sound is allowed is not a reason to keep the
 * DAC unmuted into live amplifiers.
 *
 * Once unwinding has started it runs to completion even if permission returns.
 * Turning back mid-unwind would unmute a DAC that is part way through its own
 * ramp, which is the thump the sequence exists to avoid; a fresh start costs
 * one settle time and is always clean.
 */
void hk_audio_step(hk_audio_t *seq,
                   const hk_audio_inputs_t *inputs,
                   const hk_audio_timing_t *timing);

/** What the two lines should be doing in @p state. */
hk_audio_outputs_t hk_audio_outputs(hk_audio_state_t state);

/** Short name, for logs and tests. */
const char *hk_audio_state_name(hk_audio_state_t state);

#endif /* HK_AUDIO_H */
