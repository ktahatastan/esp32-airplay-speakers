#include "hk_audio.h"

#include <stddef.h>

/** Unsigned arithmetic, so a 32-bit millisecond wrap does not read as elapsed. */
static uint32_t elapsed(uint32_t now_ms, uint32_t since_ms)
{
    return now_ms - since_ms;
}

static void enter(hk_audio_t *seq, hk_audio_state_t state, uint32_t now_ms)
{
    seq->state = state;
    seq->entered_ms = now_ms;
}

void hk_audio_init(hk_audio_t *seq, uint32_t now_ms)
{
    if (seq == NULL) {
        return;
    }
    enter(seq, HK_AUDIO_SILENT, now_ms);
}

void hk_audio_step(hk_audio_t *seq,
                   const hk_audio_inputs_t *inputs,
                   const hk_audio_timing_t *timing)
{
    if (seq == NULL) {
        return;
    }

    /* Not knowing whether sound is allowed is not a reason to keep the DAC
     * unmuted into amplifiers that cannot be muted. Missing arguments take the
     * same path as a withdrawn permission, using the state's own entry time so
     * the unwind still completes rather than stalling. */
    if (inputs == NULL || timing == NULL) {
        if (seq->state != HK_AUDIO_SILENT) {
            enter(seq, HK_AUDIO_SILENT, seq->entered_ms);
        }
        return;
    }

    const uint32_t now = inputs->now_ms;
    const bool wanted = inputs->permitted && inputs->stream_live;

    switch (seq->state) {
    case HK_AUDIO_SILENT:
        if (wanted) {
            enter(seq, HK_AUDIO_CLOCKING, now);
        }
        break;

    case HK_AUDIO_CLOCKING:
        /* The DAC is still muted here, so there is nothing to unwind: the
         * clocks can simply stop. */
        if (!wanted) {
            enter(seq, HK_AUDIO_SILENT, now);
        } else if (elapsed(now, seq->entered_ms) >= timing->clock_settle_ms) {
            enter(seq, HK_AUDIO_PLAYING, now);
        }
        break;

    case HK_AUDIO_PLAYING:
        if (!wanted) {
            /* The one transition that matters most: the DAC goes down first,
             * and the clocks are held for its ramp rather than pulled from
             * under it. */
            enter(seq, HK_AUDIO_MUTING, now);
        }
        break;

    case HK_AUDIO_MUTING:
        /* Deliberately not checking `wanted` here. Turning back mid-unwind
         * would unmute a DAC part way through its own ramp. Finishing costs
         * one settle time. */
        if (elapsed(now, seq->entered_ms) >= timing->mute_settle_ms) {
            enter(seq, HK_AUDIO_SILENT, now);
        }
        break;
    }
}

hk_audio_outputs_t hk_audio_outputs(hk_audio_state_t state)
{
    hk_audio_outputs_t out = {false, false};

    switch (state) {
    case HK_AUDIO_SILENT:
        break;
    case HK_AUDIO_CLOCKING:
        out.i2s_running = true;
        break;
    case HK_AUDIO_PLAYING:
        out.i2s_running = true;
        out.dac_unmuted = true;
        break;
    case HK_AUDIO_MUTING:
        /* The DAC is already muted while the clocks are still up. That
         * asymmetry is the whole point of having this state: the soft-mute
         * ramp needs the bit clock it is ramping against. */
        out.i2s_running = true;
        break;
    }
    return out;
}

const char *hk_audio_state_name(hk_audio_state_t state)
{
    switch (state) {
    case HK_AUDIO_SILENT:   return "SILENT";
    case HK_AUDIO_CLOCKING: return "CLOCKING";
    case HK_AUDIO_PLAYING:  return "PLAYING";
    case HK_AUDIO_MUTING:   return "MUTING";
    }
    return "INVALID";
}
