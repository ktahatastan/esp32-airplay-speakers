/**
 * @file hk_audio.h
 * @brief Turning sound on and off in an order that does not damage anything.
 *
 * Three things have to move — the I2S clocks, the DAC's soft-mute, and the
 * amplifier's shutdown pin — and the order matters in both directions.
 *
 * Coming up, the clock has to be running and stable before the DAC unmutes,
 * because a DAC unmuted into an absent or settling bit clock puts a step on its
 * output; and the DAC has to be settled before the amplifier is released,
 * because whatever the DAC is doing on its first samples gets multiplied by the
 * amplifier's gain and arrives at a tweeter whose impedance this project has
 * not yet measured.
 *
 * Going down, the order reverses and the amplifier goes first. TPA3110's
 * datasheet is explicit that the best power-off behaviour comes from asserting
 * shutdown before the supply is removed. Muting the DAC first and the
 * amplifier second would send the DAC's own transition through a live
 * amplifier — exactly the thump this sequence exists to prevent.
 *
 * The state machine is pure and takes its timings as arguments, so an entire
 * start-up and shutdown can be driven in a test in microseconds instead of
 * seconds, and so the settle times can come from G1/G3 measurements rather
 * than from a guess made here.
 *
 * The invariant worth stating once, because everything else follows from it:
 * MUTED IS THE RESTING STATE. Every path that loses permission, loses the
 * stream, or does not understand its inputs ends with the amplifier shut down,
 * and it gets there by muting the amplifier before anything else moves.
 */
#ifndef HK_AUDIO_H
#define HK_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

/** Where the output chain is in its sequence. */
typedef enum {
    HK_AUDIO_SILENT = 0,  /**< Clocks off, DAC muted, amplifier shut down */
    HK_AUDIO_CLOCKING,    /**< Clocks running, both still muted; waiting to settle */
    HK_AUDIO_DAC_LIVE,    /**< DAC unmuted, amplifier still shut down; waiting to settle */
    HK_AUDIO_PLAYING,     /**< Everything released */
    HK_AUDIO_MUTING,      /**< Amplifier shut down first; unwinding to silence */
} hk_audio_state_t;

/** What the rest of the device says about whether sound is allowed and wanted. */
typedef struct {
    bool     permitted;   /**< Calibration present, power sane, not charging */
    bool     stream_live; /**< AirPlay is delivering audio */
    uint32_t now_ms;
} hk_audio_inputs_t;

/**
 * Settle times.
 *
 * No defaults, for the usual reason: the right values come from watching the
 * rails and the outputs on a scope at G1/G3, and a number invented here would
 * be indistinguishable from a measured one. A test supplies its own.
 */
typedef struct {
    uint32_t clock_settle_ms; /**< Clocks running before the DAC may unmute */
    uint32_t dac_settle_ms;   /**< DAC unmuted before the amplifier may be released */
    uint32_t mute_settle_ms;  /**< Amplifier shut down before the DAC follows */
} hk_audio_timing_t;

/** The three lines this module drives. All three are active low in hardware. */
typedef struct {
    bool i2s_running;   /**< Clocks and data are being produced */
    bool dac_unmuted;   /**< PCM5102A XSMT released */
    bool amp_enabled;   /**< TPA3110 SD released */
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
 * Losing permission or losing the stream from any state moves to
 * ::HK_AUDIO_MUTING rather than jumping straight to ::HK_AUDIO_SILENT, so the
 * amplifier is shut down first and the rest unwinds behind it.
 *
 * A NULL @p seq, @p inputs or @p timing takes the safe path rather than doing
 * nothing: not knowing whether sound is allowed is not a reason to keep an
 * amplifier live.
 *
 * Once unwinding has started it runs to completion even if permission returns.
 * Turning back mid-unwind would release the amplifier while the DAC is part
 * way through its own transition, which is the thump the sequence exists to
 * avoid; a fresh start costs one settle time and is always clean.
 */
void hk_audio_step(hk_audio_t *seq,
                   const hk_audio_inputs_t *inputs,
                   const hk_audio_timing_t *timing);

/** What the three lines should be doing in @p state. */
hk_audio_outputs_t hk_audio_outputs(hk_audio_state_t state);

/** Short name, for logs and tests. */
const char *hk_audio_state_name(hk_audio_state_t state);

#endif /* HK_AUDIO_H */
