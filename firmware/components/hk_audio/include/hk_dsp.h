/**
 * @file hk_dsp.h
 * @brief The signal path: one mono programme in, two driver branches out.
 *
 * WHAT THE CHANNELS MEAN
 * ----------------------
 * ON THE WAY IN, `stereo` is an ordinary interleaved AirPlay stereo pair --
 * [L, R, L, R, ...], 16-bit, the left channel of the music in the even slots.
 *
 * ON THE WAY OUT, the same buffer holds something that is not stereo at all:
 *
 *     even slots -> WOOFER branch   (low-passed, its own gain, its own ceiling)
 *     odd  slots -> TWEETER branch  (high-passed, its own gain, its own ceiling)
 *
 * That is ADR-0002 and `docs/02-hardware/audio-signal-chain.md`: the DAC's two
 * analogue channels feed a BI-AMP pair, not a stereo pair, so one mono
 * programme is split by FREQUENCY and each half drives a different driver.
 * Sending real stereo down this path puts half the mix into a 60 mm cone and
 * the other half into a 25 mm dome, which is what the speaker did before this
 * module existed.
 *
 * The consequence worth stating once: AFTER hk_dsp_process(), NOTHING
 * DOWNSTREAM MAY TREAT THE TWO CHANNELS AS INTERCHANGEABLE. No balance, no
 * channel swap, no mono fold, no per-channel volume. Any of those would move
 * energy between a woofer and a tweeter. The one legitimate downstream
 * operation is a common gain applied to both slots equally -- and even that
 * lands after the limiters, so it can only make the output quieter than the
 * ceilings, never louder.
 *
 * MONO, AND WHERE "LEFT ONLY" GOES
 * --------------------------------
 * Stage 1 sums to mono. A speaker that should play only the left channel of
 * the programme (the `chan_mode` setting; four of these will sit in one room)
 * is served by the CALLER duplicating that channel into both slots before
 * calling in -- 0.5 * (L + L) is exactly L. Nothing about channel selection
 * belongs in here, and putting it here would give two places the power to
 * decide what mono means.
 *
 * THE ORDER, AND WHY IT IS THAT ORDER
 * -----------------------------------
 *   1. mono downmix        The programme is stereo; this speaker is not. Doing
 *                          it FIRST is also what makes the rest affordable:
 *                          the EQ and the subsonic filter then run once per
 *                          frame instead of twice.
 *   2. user EQ + trim      Before the protective filters, never after. A low
 *                          shelf boosting 40 Hz has to meet the subsonic
 *                          high-pass on its way out; run the other way round,
 *                          the boost would put back exactly the sub-70 Hz
 *                          content stage 3 removed, and turn a protection
 *                          filter into a suggestion.
 *   3. woofer subsonic HPF Below its resonance a driver makes excursion and no
 *                          sound. Ahead of the split so both branches are
 *                          spared content neither can use.
 *   4. LR4 split           Low branch to the woofer, high branch to the
 *                          tweeter. In phase; neither is inverted.
 *   5. per-branch gain     Level-matching the two drivers.
 *   6. per-branch limiter  LAST, so that nothing downstream of it inside this
 *                          module can undo it.
 *
 * WHY THE EQ CANNOT DEFEAT THE PROTECTION
 * ---------------------------------------
 * Not "it is not supposed to" -- three structural reasons, each one testable:
 *
 * (a) ORDER. The final operation applied to each branch is the limiter's
 *     `sample *= ceiling / |sample|`. |output| <= ceiling is a property of
 *     that last multiply alone and holds whatever the earlier stages did. An
 *     EQ boost cannot reach past a stage that runs after it.
 *
 * (b) SEPARATION. The protective numbers live in ::hk_profile_chain_t, which
 *     hk_dsp_init() copies BY VALUE into ::hk_dsp_t and which no function in
 *     this module ever writes again. The tonal numbers live in
 *     ::hk_eq_settings_t, which shares not one field with it -- no frequency,
 *     no ceiling, no branch gain. hk_dsp_set_eq() writes ::hk_dsp_t::eq and
 *     nothing else. There is no code path from a user setting to a corner
 *     frequency, and that is a claim a reader can check with grep.
 *
 * (c) BOUNDED AUTHORITY. A band is capped at +/-12 dB and the master trim can
 *     only cut. That bound is NOT what makes the speaker safe -- (a) already
 *     does -- it is what keeps the signal from sitting so far above the
 *     ceilings that the limiter never lets go and the speaker just sounds
 *     compressed.
 *
 * These are also why (b) is not merely tidiness: PRD-008 says a user reset
 * must not erase factory calibration, and the two live in different NVS
 * partitions for that reason (hk_storage.h). The struct boundary here is the
 * same wall, one layer up.
 *
 * WHAT HAPPENS WHEN THERE IS NO PROFILE
 * -------------------------------------
 * The buffer is filled with silence and hk_dsp_process() returns false.
 *
 * Full-band passthrough is not on the table: it would put the whole programme
 * into a 25 mm dome whose Fs has never been located, behind a single 10 uF
 * capacitor, at whatever level the stream happens to carry.
 *
 * The objection to silence is real -- a speaker that goes mute on a bad byte
 * is a bad speaker -- and it does not apply here, for two reasons. First, a
 * bad byte cannot cause this: the profile is judged ONCE, in
 * hk_dsp_init(), and no audio content can change the verdict; the only way to
 * reach the refusal is to have no usable calibration, which is a state of the
 * device rather than of the music. Second, the device is already silent in
 * that state by design: hk_storage_audio_permitted() is false, hk_audio_step()
 * therefore drives the sequence to muted, and the amplifier is shut down. This
 * module refusing is not a new failure mode; it is the DSP agreeing with the
 * gate that already exists rather than holding a second, softer opinion.
 *
 * And the return value is what stops silence being the whole answer. It
 * follows hk_profile_ceiling_at()'s precedent exactly: "do not play" is not
 * the same as "play silence". A caller that writes the zeroed buffer to a live
 * amplifier has misread this contract -- false means take the amplifier down
 * and say why, and hk_dsp_refusal_name() supplies the why, so the owner meets
 * a speaker that reports it is not calibrated rather than a dead box.
 *
 * BAD TONAL DATA IS THE OPPOSITE CASE and is handled the opposite way: a band
 * that will not design is bypassed and the speaker plays on. Protective data
 * that cannot be honoured stops everything; tonal data that cannot be honoured
 * stops one tone control.
 *
 * LATENCY: ZERO ADDED SAMPLES, WHICH IS NOT THE SAME AS ZERO DELAY. Nothing
 * here buffers: every stage is a recursive filter or a memoryless multiply, and
 * the limiter has no lookahead by deliberate design (hk_limiter.h). So no frame
 * waits for a later frame, and the pipeline depth the timing engine models is
 * unchanged.
 *
 * The filters still have GROUP DELAY, because a minimum-phase filter does.
 * Measured off this code on the woofer branch with the provisional 70 Hz
 * subsonic corner: 3.84 ms at 50 Hz, 3.35 ms at 70 Hz, 1.93 ms at 100 Hz,
 * 0.52 ms at 200 Hz, and under 0.2 ms above 500 Hz. Those numbers are larger
 * than ADR-0007's 1 ms group-synchronisation budget, and saying the budget is
 * "untouched" would be wrong.
 *
 * What actually saves it is that the delay is COMMON MODE. Four speakers
 * running the same protective corners are all delayed by the same amount at the
 * same frequency, and a delay every room shares is not a synchronisation error.
 * That holds only while the corners are shared: each speaker carries its own
 * `factory_cal`, so per-unit subsonic corners are physically possible, and
 * moving one box from 70 Hz to 50 Hz costs 0.57 ms of relative delay -- over
 * half the budget before the network is asked for anything. THE SUBSONIC CORNER
 * SHOULD THEREFORE BE A PRODUCT-WIDE CONSTANT even where the ceilings are
 * per-unit. The crossover corner is cheap by comparison: 4000 -> 2000 Hz moves
 * it by 0.15 ms.
 */
#ifndef HK_DSP_H
#define HK_DSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hk_biquad.h"
#include "hk_eq.h"
#include "hk_limiter.h"
#include "hk_profile.h"

/** Why a chain was refused, so a bench or a display can say which number. */
typedef enum {
    HK_DSP_OK = 0,
    HK_DSP_NO_CHAIN,    /**< No chain was supplied; the device is uncalibrated */
    HK_DSP_BAD_RATE,    /**< Sample rate absent, or not the one the chain was built at */
    HK_DSP_BAD_FILTER,  /**< A protective section does not describe a stable filter */
    HK_DSP_BAD_GAIN,    /**< A branch gain is outside (0, 1] */
    HK_DSP_BAD_LIMITER, /**< A limiter refused its configuration */
} hk_dsp_refusal_t;

/**
 * The whole signal path's state. The caller owns it; no globals.
 *
 * @p chain is a COPY, not a pointer. Two reasons, and the second is the
 * important one: the caller's profile may be freed or rebuilt at any time, and
 * a protective coefficient that could change under a running audio task is a
 * protective coefficient that is not guaranteed.
 */
typedef struct {
    bool               ready;
    hk_dsp_refusal_t   refusal;
    float              sample_rate_hz;

    hk_profile_chain_t chain;      /**< Protective. Written once, at init */

    hk_eq_t            eq;         /**< Tonal. May be replaced while playing */

    hk_biquad_state_t  hpf_state;
    hk_lr4_state_t     low_state;
    hk_lr4_state_t     high_state;
    hk_limiter_t       woofer_limit;
    hk_limiter_t       tweeter_limit;
} hk_dsp_t;

/**
 * Build the path.
 *
 * @param chain     from hk_profile_build(). NULL means uncalibrated, which is
 *                  a refusal rather than a reason to invent defaults.
 * @param eq        tonal settings. NULL is legal and means flat.
 * @param sample_rate_hz must match the rate @p chain was built at -- the
 *                  limiter's release times were converted to samples using it,
 *                  so a mismatch is a chain built for another device state.
 *
 * @return false when the path cannot be trusted to protect the drivers.
 *         hk_dsp_refusal() then names which number was wrong.
 */
bool hk_dsp_init(hk_dsp_t *dsp, const hk_profile_chain_t *chain,
                 const hk_eq_settings_t *eq, float sample_rate_hz);

/**
 * Process one interleaved stereo block IN PLACE.
 *
 * In place because that is the buffer the output backend already owns, and
 * because owning the buffer is the point: the vendored backend hands the SAME
 * silence buffer to its LED feed and then to I2S without refilling it, so an
 * in-place stage bolted in there would read back its own previous output on
 * every underrun frame. With an EQ boost in the loop that path GROWS rather
 * than decays. The third backend exists so this function reads a buffer that
 * was filled this iteration (ADR-0013 forbids editing the vendored tree).
 *
 * @param stereo interleaved 16-bit, `frames` frames, two slots per frame.
 * @param frames frames, NOT samples.
 *
 * @return true when the block carries programme. FALSE MEANS DO NOT PLAY: the
 *         buffer has been zeroed so nothing unsafe can be written by mistake,
 *         but zeros through a live amplifier are silence, and the state this
 *         reports is not silence -- it is an amplifier that should be shut
 *         down and a reason that should be logged.
 */
bool hk_dsp_process(hk_dsp_t *dsp, int16_t *stereo, size_t frames);

/**
 * Replace the tonal settings on a running path.
 *
 * Touches ::hk_dsp_t::eq only. It cannot make a ready path unready and it
 * cannot make an unready one ready, because it does not go near a protective
 * number -- which is the whole of guarantee (b) in this file's header.
 *
 * Safe to call mid-stream. A band whose coefficients come out unchanged keeps
 * its filter memory, so editing one control does not put a step into the two
 * the owner did not touch; a band that really changed restarts from rest. See
 * hk_dsp.c for why hk_eq_build() cannot do this itself.
 *
 * @return true if every band built. False means a band was bypassed, or that
 *         @p dsp was NULL or the settings could not be read at all; the path
 *         keeps playing either way.
 */
bool hk_dsp_set_eq(hk_dsp_t *dsp, const hk_eq_settings_t *eq);

/**
 * Clear every filter's memory and let the limiters back to unity gain.
 *
 * For a stream flush or a rate change, where the samples that follow have no
 * relationship to the ones before and the filter state is describing music
 * that is no longer playing. Does not re-read the profile.
 */
void hk_dsp_reset(hk_dsp_t *dsp);

/** Whether the path may be used. */
bool hk_dsp_ready(const hk_dsp_t *dsp);

/** Why it may not. ::HK_DSP_OK when it may. */
hk_dsp_refusal_t hk_dsp_refusal(const hk_dsp_t *dsp);

/** A one-word reason, for a log line, a display or a bench report. */
const char *hk_dsp_refusal_name(hk_dsp_refusal_t refusal);

/**
 * How many biquad sections a frame passes through right now.
 *
 * Five are protective and always run (one subsonic section, two per LR4
 * branch); the rest are whatever the owner has turned on. Exposed so the cost
 * of the chain is a number the firmware can report rather than one that has to
 * be counted by hand from the source.
 */
size_t hk_dsp_biquads_per_frame(const hk_dsp_t *dsp);

#endif /* HK_DSP_H */
