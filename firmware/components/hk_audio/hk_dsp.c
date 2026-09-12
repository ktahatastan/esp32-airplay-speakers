#include "hk_dsp.h"

#include <float.h>
#include <math.h>
#include <string.h>

/** Sections that always run: two in the subsonic filter, two per LR4 branch. */
#define HK_DSP_PROTECTIVE_BIQUADS 6u

/**
 * Push a state word that has decayed into the subnormal range to zero.
 *
 * MEASURED, not precautionary. Drive the chain with programme, then feed it
 * silence the way the backend actually does -- it calls hk_dsp_process() on
 * every silence frame on purpose, so the filters ring out across a gap instead
 * of freezing mid-decay. The subsonic section's poles sit at radius 0.993, so
 * its memory takes about 12 000 samples to fall from unity to FLT_MIN and then
 * STOPS DECAYING CLEANLY: subnormals carry fewer mantissa bits, the recurrence
 * turns into a limit cycle down there, and the state never reaches zero. On the
 * bench harness that was 373 of 400 silence blocks holding subnormal state,
 * from 216 ms after the music stopped until the end of the run -- i.e. for as
 * long as the speaker is idle, which is most of its life.
 *
 * Two costs, and the second is the one that matters. It is arithmetic nobody
 * needs, on a value 758 dB below full scale. And subnormal operands are the
 * classic audio-DSP performance cliff: a core that handles them in microcode or
 * traps to software pays orders of magnitude per operation, and this is the
 * audio task, inside the frame deadline. Whether the ESP32-S3's FPU does that
 * is NOT established here -- no board has been touched -- so this is written as
 * the cheap way to make the question moot rather than as a fix for a measured
 * stall.
 *
 * Flushing is safe in the way that matters for this module: FLT_MIN is far
 * below one LSB of the int16 output, so no sample can change value. It is done
 * once per block rather than per sample, which bounds the subnormal arithmetic
 * to at most one block per silence episode and costs 19 compares per 352
 * frames (six protective sections, three tonal ones, the supply detector).
 */
static void flush_subnormal(float *value)
{
    if (*value != 0.0f && fabsf(*value) < FLT_MIN) {
        *value = 0.0f;
    }
}

static void flush_state(hk_biquad_state_t *state)
{
    flush_subnormal(&state->z1);
    flush_subnormal(&state->z2);
}

/**
 * Every recursive memory in the path, protective and tonal alike.
 *
 * The supply detector is a one-pole recurrence too: during silence it decays
 * geometrically towards zero and would sit in the subnormal range for the
 * same reason the filters did. The delay rings are not flushed -- they hold
 * signal samples, not a recurrence, and a subnormal that enters one leaves it
 * a few samples later unchanged.
 */
static void flush_denormals(hk_dsp_t *dsp)
{
    for (size_t i = 0; i < 2u; i++) {
        flush_state(&dsp->hpf_state.section[i]);
        flush_state(&dsp->low_state.section[i]);
        flush_state(&dsp->high_state.section[i]);
    }
    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        flush_state(&dsp->eq.stage[b].state);
    }
    flush_subnormal(&dsp->supply_limit.mean_sq);
}

/** Finite and strictly positive. NaN fails every comparison, so ask directly. */
static bool positive(float value)
{
    return isfinite(value) && value > 0.0f;
}

/** A linear gain: above zero and no more than full scale. */
static bool unit_range(float value)
{
    return positive(value) && value <= 1.0f;
}

/**
 * Full scale is 32768 on the way in and 32767 on the way out, and the
 * asymmetry is real rather than sloppy: int16 reaches -32768 but not +32767's
 * mirror, so dividing by 32768 maps the whole input range into [-1, 1] with no
 * value able to exceed it, while multiplying by 32767 on the way out keeps the
 * positive peak representable. The alternative -- 32767 both ways -- lets a
 * full-negative input arrive as -1.00003, which is outside the range every
 * stage after it assumes.
 */
static float from_i16(int16_t sample)
{
    return (float)sample * (1.0f / 32768.0f);
}

static int16_t to_i16(float value)
{
    /* A NaN reaching the cast is undefined behaviour, and NaN is reachable:
     * the limiter deliberately leaves a non-finite sample alone rather than
     * repairing it (hk_limiter.c), because repairing it is not that stage's
     * job. It is this one's -- this is the last place before the DAC. */
    if (!isfinite(value)) {
        return 0;
    }
    if (value >= 1.0f) {
        return 32767;
    }
    if (value <= -1.0f) {
        return -32768;
    }
    const float scaled = value * 32767.0f;
    return (int16_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

/** Every protective section: finite coefficients, both poles inside the unit circle. */
static bool chain_filters_stable(const hk_profile_chain_t *chain)
{
    for (size_t i = 0; i < 2u; i++) {
        if (!hk_biquad_stable(&chain->woofer_hpf.section[i]) ||
            !hk_biquad_stable(&chain->woofer_low.section[i]) ||
            !hk_biquad_stable(&chain->tweeter_high.section[i])) {
            return false;
        }
    }
    return true;
}

/**
 * One sample through a branch's alignment ring.
 *
 * A ring of exactly @p delay entries: what comes out is what went in @p delay
 * samples ago. Zero delay is a bypass and never touches the ring, so a
 * profile with no correction costs nothing here.
 */
static float delay_step(float *ring, uint32_t *index, uint32_t delay, float sample)
{
    if (delay == 0u) {
        return sample;
    }
    const float out = ring[*index];
    ring[*index] = sample;
    *index = (*index + 1u) % delay;
    return out;
}

/** The delay pair the profile validator already refused; checked again here
 * because a chain is a plain struct and could have come from anywhere. */
static bool delays_valid(const hk_profile_chain_t *chain)
{
    if (chain->woofer_delay_samples > HK_PROFILE_DELAY_MAX_SAMPLES ||
        chain->tweeter_delay_samples > HK_PROFILE_DELAY_MAX_SAMPLES) {
        return false;
    }
    /* One branch relative to the other. Both delayed is a latency wearing a
     * correction's name, and it would also make the latency paragraph in
     * hk_dsp.h false. */
    return chain->woofer_delay_samples == 0u || chain->tweeter_delay_samples == 0u;
}

static bool fail(hk_dsp_t *dsp, hk_dsp_refusal_t why)
{
    dsp->ready = false;
    dsp->refusal = why;
    return false;
}

bool hk_dsp_init(hk_dsp_t *dsp, const hk_profile_chain_t *chain,
                 const hk_eq_settings_t *eq, float sample_rate_hz)
{
    if (dsp == NULL) {
        return false;
    }
    memset(dsp, 0, sizeof(*dsp));

    /* Flat, and built before any refusal can return. An unready path still has
     * a coherent equaliser rather than a zeroed struct that would look like a
     * filter with all-zero coefficients if anyone ever ran it. */
    (void)hk_eq_build(&dsp->eq, NULL, sample_rate_hz);

    if (chain == NULL) {
        return fail(dsp, HK_DSP_NO_CHAIN);
    }
    if (!positive(sample_rate_hz)) {
        return fail(dsp, HK_DSP_BAD_RATE);
    }

    /* The chain arrives as a plain struct and could have come from anywhere.
     * hk_profile_build() would have designed stable sections, but nothing in
     * the type system says this chain came from there -- and hk_biquad_stable()
     * documents itself as existing for exactly this case. */
    if (!chain_filters_stable(chain)) {
        return fail(dsp, HK_DSP_BAD_FILTER);
    }
    if (!unit_range(chain->woofer_gain) || !unit_range(chain->tweeter_gain)) {
        return fail(dsp, HK_DSP_BAD_GAIN);
    }
    /* Exactly +1 or -1: a polarity is not a gain, and a sign of 0.5 or NaN
     * would be a level change hiding in a field that is only allowed to flip. */
    if (chain->tweeter_sign != 1.0f && chain->tweeter_sign != -1.0f) {
        return fail(dsp, HK_DSP_BAD_GAIN);
    }
    if (!delays_valid(chain)) {
        return fail(dsp, HK_DSP_BAD_DELAY);
    }

    /* A chain built at another sample rate is not merely mistuned. Its
     * limiters' release and hold times, and the supply detector's window,
     * were converted to SAMPLE COUNTS using that other rate, so running it
     * here would give recovery times wrong by the ratio -- silently, since
     * nothing downstream can tell. */
    const uint32_t rate = (uint32_t)sample_rate_hz;
    if (chain->woofer_limit.sample_rate != rate ||
        chain->tweeter_limit.sample_rate != rate ||
        chain->supply_limit.sample_rate != rate) {
        return fail(dsp, HK_DSP_BAD_RATE);
    }

    dsp->chain = *chain; /* By value: see hk_dsp.h on why this is a copy. */
    dsp->sample_rate_hz = sample_rate_hz;

    if (!hk_limiter_init(&dsp->woofer_limit, &dsp->chain.woofer_limit) ||
        !hk_limiter_init(&dsp->tweeter_limit, &dsp->chain.tweeter_limit)) {
        return fail(dsp, HK_DSP_BAD_LIMITER);
    }
    if (!hk_supply_limiter_init(&dsp->supply_limit, &dsp->chain.supply_limit)) {
        return fail(dsp, HK_DSP_BAD_SUPPLY);
    }

    /* Tonal settings come last and cannot change the verdict above. NULL is
     * flat, and a band that will not design is bypassed, not fatal. */
    (void)hk_eq_build(&dsp->eq, eq, sample_rate_hz);

    dsp->ready = true;
    dsp->refusal = HK_DSP_OK;
    return true;
}

bool hk_dsp_set_eq(hk_dsp_t *dsp, const hk_eq_settings_t *eq)
{
    if (dsp == NULL) {
        return false;
    }
    /* hk_eq_build() zeroes the whole ::hk_eq_t, FILTER MEMORY INCLUDED. That is
     * right for a fresh struct -- it is what makes the function safe to call on
     * uninitialised storage, which hk_dsp_init() relies on -- and wrong for a
     * path that is playing. Dropping a running section's memory is a step
     * discontinuity scaled by the signal's current amplitude, which is exactly
     * the artefact the vendored backend's volume ramp exists to avoid, and it
     * lands on a band the owner did not even touch: turning the treble knob
     * would click the bass shelf.
     *
     * Measured properly -- as the deviation from the path that had the new
     * settings all along, not as the step between two adjacent samples, which
     * is mostly the waveform's own slope -- adding a 10 kHz shelf while a
     * 120 Hz tone plays through a +12 dB low shelf threw the output 9029 LSB
     * off course: 27.6% of full scale, -11.2 dBFS, about three quarters of the
     * signal's own amplitude. With the carry-over below it is 980 LSB, and what
     * is left is the new band starting from rest, which is real.
     *
     * It also contradicts this module's own rule for when memory may be
     * cleared. hk_dsp_reset() is documented for "a stream flush or a rate
     * change, where the samples that follow have no relationship to the ones
     * before". An EQ edit is the opposite case: the programme is continuous and
     * only the response is meant to change.
     *
     * So the state is carried across the rebuild for every band whose
     * coefficients came out BIT-IDENTICAL -- which is every band the owner did
     * not move. A band that genuinely changed keeps the zeroed memory: some
     * transient is unavoidable there, and starting that section from rest is
     * the bounded choice rather than the clever one.
     *
     * Restoring only into a stage hk_eq_build() has just marked active is what
     * keeps this from reaching past its remit: a rejected or bypassed band has
     * no coefficients to match, so it cannot be handed stale memory. */
    hk_eq_stage_t before[HK_EQ_BANDS];
    memcpy(before, dsp->eq.stage, sizeof(before));

    /* ::hk_dsp_t::eq and nothing else. No branch of this function can reach a
     * corner frequency, a branch gain or a ceiling, which is what makes
     * guarantee (b) in hk_dsp.h a fact about the code rather than a wish. */
    if (!hk_eq_build(&dsp->eq, eq, dsp->sample_rate_hz)) {
        return false;
    }

    for (size_t b = 0; b < (size_t)HK_EQ_BANDS; b++) {
        if (before[b].active && dsp->eq.stage[b].active &&
            memcmp(&before[b].coeffs, &dsp->eq.stage[b].coeffs,
                   sizeof(before[b].coeffs)) == 0) {
            dsp->eq.stage[b].state = before[b].state;
        }
    }

    return dsp->eq.rejected == 0u;
}

void hk_dsp_reset(hk_dsp_t *dsp)
{
    if (dsp == NULL) {
        return;
    }
    memset(&dsp->hpf_state, 0, sizeof(dsp->hpf_state));
    memset(&dsp->low_state, 0, sizeof(dsp->low_state));
    memset(&dsp->high_state, 0, sizeof(dsp->high_state));
    memset(dsp->woofer_delay, 0, sizeof(dsp->woofer_delay));
    memset(dsp->tweeter_delay, 0, sizeof(dsp->tweeter_delay));
    dsp->woofer_delay_index = 0u;
    dsp->tweeter_delay_index = 0u;
    hk_eq_reset(&dsp->eq);

    /* Re-init rather than poking gain back to 1: it re-derives release_coeff
     * and hold_samples from the stored config, so a reset cannot leave a
     * limiter half-configured. The supply stage keeps its config and coeff
     * and only forgets the signal, which its own reset does. */
    (void)hk_limiter_init(&dsp->woofer_limit, &dsp->chain.woofer_limit);
    (void)hk_limiter_init(&dsp->tweeter_limit, &dsp->chain.tweeter_limit);
    hk_supply_limiter_reset(&dsp->supply_limit);
}

bool hk_dsp_process(hk_dsp_t *dsp, int16_t *stereo, size_t frames)
{
    if (dsp == NULL || stereo == NULL) {
        return false;
    }

    /* Every condition checked once per block, not once per sample.
     *
     * The limiter checks are not redundant with `ready`. Below, the inner
     * loop calls hk_limiter_step() and hk_supply_limiter_step() directly
     * rather than a block function, because the branches are interleaved in
     * one buffer and there is no contiguous run to hand one. A block function
     * refuses when its stage is not ready; the step functions return a gain of
     * 1.0 -- which is exactly the full-level passthrough this whole module
     * exists to prevent. So the refusal that the block function would have
     * made is made here instead, at the same granularity, before a single
     * sample moves. */
    if (!dsp->ready || !dsp->woofer_limit.ready || !dsp->tweeter_limit.ready ||
        !dsp->supply_limit.ready) {
        memset(stereo, 0, frames * 2u * sizeof(int16_t));
        return false;
    }

    /* The supply stage's one square root per block, before a sample moves. */
    hk_supply_limiter_block_begin(&dsp->supply_limit, frames);

    for (size_t i = 0; i < frames; i++) {
        const size_t left = 2u * i;
        const size_t right = left + 1u;

        /* 1. Mono. The programme is stereo; this speaker is not (ADR-0002). */
        float mono = 0.5f * (from_i16(stereo[left]) + from_i16(stereo[right]));

        /* 2. Tonal: the owner's EQ and master trim. Ahead of stage 3 so a bass
         *    boost still has to get past the subsonic filter. */
        mono = hk_eq_process_one(&dsp->eq, mono);

        /* 3. Subsonic: content the woofer would turn into excursion, not
         *    sound. Fourth order, two sections. */
        mono = hk_lr4_process_one(&dsp->chain.woofer_hpf, &dsp->hpf_state, mono);

        /* 4. The split. In phase -- neither branch is inverted here (hk_biquad.h). */
        float woofer = hk_lr4_process_one(&dsp->chain.woofer_low, &dsp->low_state, mono);
        float tweeter = hk_lr4_process_one(&dsp->chain.tweeter_high, &dsp->high_state, mono);

        /* 5. Level-match the two drivers. */
        woofer *= dsp->chain.woofer_gain;
        tweeter *= dsp->chain.tweeter_gain;

        /* 5b. Acoustic-centre alignment: at most one of these is not a bypass. */
        woofer = delay_step(dsp->woofer_delay, &dsp->woofer_delay_index,
                            dsp->chain.woofer_delay_samples, woofer);
        tweeter = delay_step(dsp->tweeter_delay, &dsp->tweeter_delay_index,
                             dsp->chain.tweeter_delay_samples, tweeter);

        /* 5c. Polarity, as the G2 sum measurement decided it. */
        tweeter *= dsp->chain.tweeter_sign;

        /* 5d. The adapter budget: one gain over both branches, so the
         *     crossover sum keeps its shape while the rail is spared. */
        const float supply_gain = hk_supply_limiter_step(&dsp->supply_limit, woofer, tweeter);
        woofer *= supply_gain;
        tweeter *= supply_gain;

        /* 6. Protection, last. |out| <= ceiling is a property of this multiply
         *    alone, so no earlier stage -- EQ included -- can reach past it. */
        woofer *= hk_limiter_step(&dsp->woofer_limit, woofer);
        tweeter *= hk_limiter_step(&dsp->tweeter_limit, tweeter);

        stereo[left] = to_i16(woofer);
        stereo[right] = to_i16(tweeter);
    }

    /* Once per block, not per sample: see flush_subnormal(). */
    flush_denormals(dsp);
    return true;
}

bool hk_dsp_ready(const hk_dsp_t *dsp)
{
    return dsp != NULL && dsp->ready;
}

hk_dsp_refusal_t hk_dsp_refusal(const hk_dsp_t *dsp)
{
    if (dsp == NULL) {
        return HK_DSP_NO_CHAIN;
    }
    return dsp->refusal;
}

const char *hk_dsp_refusal_name(hk_dsp_refusal_t refusal)
{
    switch (refusal) {
    case HK_DSP_OK:          return "ok";
    case HK_DSP_NO_CHAIN:    return "uncalibrated";
    case HK_DSP_BAD_RATE:    return "rate";
    case HK_DSP_BAD_FILTER:  return "filter";
    case HK_DSP_BAD_GAIN:    return "gain";
    case HK_DSP_BAD_LIMITER: return "limiter";
    case HK_DSP_BAD_DELAY:   return "delay";
    case HK_DSP_BAD_SUPPLY:  return "supply";
    default:                 return "unknown";
    }
}

size_t hk_dsp_biquads_per_frame(const hk_dsp_t *dsp)
{
    if (dsp == NULL || !dsp->ready) {
        return 0;
    }
    return (size_t)HK_DSP_PROTECTIVE_BIQUADS + hk_eq_active_bands(&dsp->eq);
}
