/**
 * @file hk_airplay_output_i2s.c
 * @brief The third output backend: I2S to the PCM5102A, through this project's
 *        crossover, protective filters and limiters. The product's output
 *        backend since ADR-0022; the vendored passthrough is for the
 *        development card and the bench exception only.
 *
 * This is a SHADOW of vendor/audio/audio_output.c, at the commit ADR-0007 pins
 * (rbouteiller/airplay-esp32 @ 38027441ff4327611d26153a8e8b06636cdf009f,
 * upstream v0.2.0). It is not a patch of it: ADR-0013 forbids changing a line
 * of the vendored tree, and upstream's own arrangement -- weak defaults in
 * audio_output_common.c, exactly one backend supplying the rest, chosen by
 * Kconfig -- already has a slot for a third implementation. This is that slot.
 *
 * Everything that is not the DSP is deliberately the same as the vendored file
 * and is copied from it: the DMA geometry, the submitted/sent playout cursor
 * and its ISR, the latency model and the live pipeline report, the task
 * priority and core pinning, the volume ramp, and the blocking-write underrun
 * pacing. Each of those carries a comment naming what it was copied from,
 * because the next upstream update has to be reviewed against this file rather
 * than merged over it. scripts/check_vendor_output_shadow.py is what makes that
 * review happen instead of being remembered.
 *
 *
 * WHY THIS FILE EXISTS AT ALL, rather than a DSP call inside the vendored one
 * ==========================================================================
 * vendor/audio/audio_output.c:249-255 is the obvious hook, and it is a trap.
 * On a receiver underflow it hands the SAME `silence` buffer to
 * led_audio_feed() and then writes that buffer to I2S -- and never refills it.
 * It stays silent only because nothing along that path writes to it.
 *
 * hk_dsp_process() works in place, on the buffer the backend already owns. Bolt
 * it in there and that buffer stops being silence: frame N's output becomes
 * frame N+1's input. The loop gain is 0.5 * |EQ| at each frequency (one half
 * from the LR4 branch split, the rest from whatever the equaliser is doing), so
 * anywhere the owner asks for bass boost the loop gain exceeds one and the
 * "silence" frame GROWS on every underrun instead of decaying. That is a
 * runaway into an amplifier driving a tweeter whose Fs has not been measured.
 *
 * A guard would work. Owning the buffers removes the defect. Every buffer this
 * file hands to hk_dsp_process() is completely written first, in the same loop
 * iteration, by something that is not the DSP:
 *
 *   pcm            filled by audio_receiver_read()
 *   resample_buf   filled by audio_resample_process()
 *   silence        zeroed by memset, unconditionally, every time it is used
 *
 * The third is the one upstream gets wrong, and the memset is the whole fix.
 * There is no arrangement of underruns that can feed this backend its own
 * output, because there is no buffer here whose previous contents survive into
 * the next frame.
 *
 *
 * WHAT THIS BACKEND WILL AND WILL NOT DO
 * ======================================
 * The DSP holds the crossover, the subsonic filter, the alignment delay and
 * polarity, the supply-budget stage and the two peak limiters, in the order
 * hk_dsp.h gives. It is ready only when a calibration profile has been read
 * AND built, and one function does both: hk_profile_load(). Until then this
 * backend clocks I2S, drains the receiver and writes DIGITAL ZERO. It does not
 * fall back to passing audio through: an unfiltered full-range signal on the
 * tweeter branch is precisely the damage the profile exists to prevent.
 * hk_dsp.h makes the same refusal for the same reason, and says the thing
 * worth repeating here -- writing zeros to a live amplifier is not the answer
 * to "do not play". It is not the answer here either, and it does not have to
 * be: hk_storage_audio_permitted() is already false in exactly this state, so
 * hk_audio_step() has already driven the mute sequence and the DAC's XSMT is
 * already held down. That is true of BOTH ways of arriving here -- no profile
 * stored, and a profile stored but refused -- because hk_main judges the
 * stored blob at boot with the same hk_profile_load() this file calls and
 * pushes the verdict into hk_storage. A blob this backend would refuse is a
 * blob the gate has already refused, so the two cannot disagree about it
 * (ADR-0022). This backend agreeing with that gate costs nothing; disagreeing
 * with it would be the bug. The one build that can arrive here with the DAC
 * live is the bench exception (CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE) without
 * the provisional profile, which lifts the absence refusal by declaration --
 * and only that one: a profile that is present but refused stays refused on
 * the bench too (hk_storage.c). See its Kconfig help; nothing tracked builds
 * that way.
 *
 * The gate is the protection, not the zeros. The XH-A232 has no mute input,
 * so the amplifiers are live whenever VIN is; what the gate holds is the DAC
 * (hk_audio_hw.c), and digital zero through a muted DAC is the quietest thing
 * this firmware can produce.
 *
 * See the Kconfig help text, which says the same thing where somebody selecting
 * this backend will read it.
 */

#include "audio_output.h"

#include "audio_receiver.h"
#include "audio_resample.h"
#include "dac.h"
#include "led.h"
#include "rtsp_server.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <string.h>
#include <stdlib.h>

/* This project's own modules. The DSP is the reason this backend exists; the
 * other two are what it takes to hand the DSP an honest chain. */
#include "hk_dsp.h"
#include "hk_eq.h"
#include "hk_profile.h"
#include "hk_storage.h"

#define TAG "hk_out_dsp"

/* --- Copied from vendor/audio/audio_output.c:33-56, values unchanged. ------
 *
 * FRAME_SAMPLES is upstream's name and it counts stereo FRAMES, not samples;
 * every arithmetic use in that file multiplies it by 2 for the channel pair.
 * Kept misnamed on purpose so the two files diff cleanly. */
#define I2S_SCK_PIN   CONFIG_I2S_SCK_IO
#define I2S_BCK_PIN   CONFIG_I2S_BCK_IO
#define I2S_LRCK_PIN  CONFIG_I2S_WS_IO
#define I2S_DOUT_PIN  CONFIG_I2S_DO_IO
#define OUTPUT_RATE   CONFIG_OUTPUT_SAMPLE_RATE_HZ
#define FRAME_SAMPLES 352

#if CONFIG_I2S_GND_IO >= 0
#define I2S_GND_PIN CONFIG_I2S_GND_IO
#endif
#if CONFIG_I2S_VCC_IO >= 0
#define I2S_VCC_PIN CONFIG_I2S_VCC_IO
#endif

/* DMA ring geometry. Total DMA latency in samples is DESC_NUM x FRAME_NUM.
 * Identical to the vendored backend, because the timing engine's model of the
 * pipeline (audio_timing.c) is calibrated against these numbers and a backend
 * that quietly used different ones would move every frame's early/late
 * decision. */
#define I2S_DMA_DESC_NUM  8
#define I2S_DMA_FRAME_NUM 256

/* Max output frames after resampling one input frame. Copied verbatim. */
#define MAX_RESAMPLE_FRAMES \
  ((size_t)((FRAME_SAMPLES + 2) * ((double)OUTPUT_RATE / 44100) + 16))

#if CONFIG_FREERTOS_UNICORE
#define PLAYBACK_CORE 0
#else
#define PLAYBACK_CORE 1
#endif

/* ==========================================================================
 * The DSP, and what it takes to build one
 * ========================================================================== */

/* The path's state lives here rather than in hk_audio, because hk_dsp_t is
 * caller-owned by design (hk_dsp.h: "no globals") and the playback task is the
 * only thing that touches it per frame. One instance, one owner, one writer. */
static hk_dsp_t s_dsp;

/**
 * The supply voltage a limiter ceiling has to be translated to.
 *
 * The four amplifiers run from one fixed 24 V adapter (ADR-0020), and the
 * firmware has no way to measure it, so the supply the ceilings are scaled to
 * is the configured nominal: CONFIG_HK_SUPPLY_MV, 24 V by default. That is a
 * statement about the adapter the wiring plan names, not a reading, and it is
 * stated once in Kconfig so that a different adapter is a one-line change
 * rather than a hunt through the source.
 *
 * What the scaling does, and what it does not. hk_profile_ceiling_at()
 * multiplies each peak ceiling by min(1, reference / supply): a ceiling
 * listened to at 12 V on the bench is halved for this 24 V build, and a
 * supply below the reference never raises one. That errs quiet where it was
 * written for, and it is all it does. This file used to say the halving keeps
 * "the volts at the driver what the bench heard", and that is withdrawn: the
 * TPA3110D2 is a fixed-gain amplifier whose output is gain x input until the
 * rail clips (SLOS528F, Table 3 -- the same 1 Vrms in gives 23.5 Vpp from
 * 12 V and 27.7 Vpp from 24 V, the 12 V figure being the rail clipping), and
 * the branch gains are not scaled at all. So where the bench level did not
 * clip the 12 V rail, the halved ceiling is half the bench voltage; where it
 * did -- possible at an unread gain strapping, bench item C3 -- the 24 V rail
 * clips later and the driver can see more than the bench's clipped level at
 * the same slider. Which case applies is what C3 decides, and until it is
 * read a profile built for the adapter is played staged, at low level, on one
 * amplifier and one pair (AGENTS.md).
 *
 * The error cases in the CONFIGURED value are still not symmetric: a lower
 * adapter left unconfigured errs quiet (the rail clips first), a higher one is
 * the danger -- which is why G1 measures the adapter's no-load output against
 * the amplifier's 26 V maximum before it is connected. The mechanism is one
 * field and one multiply, and that is what it buys.
 */
static float supply_mv_now(void);

#if CONFIG_HK_BENCH_PROVISIONAL_PROFILE
/** The supply voltage the bench ceilings were listened to at. */
#define HK_BENCH_REFERENCE_SUPPLY_MV 12000

/**
 * A profile nobody measured, so the speaker can be LISTENED to.
 *
 * Two of these numbers are measurements and the rest are not, and the
 * difference is the whole reason this function is behind its own Kconfig
 * symbol rather than being a default. Measured on 2026-09-08, per the operator
 * record (docs/02-hardware/driver-measurements.md): the woofer's DC resistance
 * is 4.0 ohm and the tweeter's 3.5 ohm, both 4 ohm class. This file carried
 * 3.7 for the tweeter until 2026-09-12; the record says 3.5, the record is the
 * source, so the record wins -- and only the operator can say which reading
 * was the tweeter's, so the line waits on a re-read. The profile carries the
 * value and never computes with it, which is why a wrong one is a wrong
 * source record rather than a wrong protection. Everything else below is a
 * choice derived from what the 2026-09-12 coarse sweep found (owner, DAC
 * sweep mode, docs/02-hardware/driver-measurements.md section 11): the
 * woofer's impedance peaks at about 50 Hz (+/-13 %), and the tweeter shows a
 * flat ~2 x Re from 1 kHz to 4.9 kHz with no peak the 10 mV/div scope could
 * resolve -- a heavily damped dome whose Fs is a range, 1-3 kHz, with the
 * 2026-09-08 bump at 2000-2450 Hz the best point estimate.
 *
 * WHY EACH NUMBER IS WHAT IT IS:
 *
 * crossover 3500 Hz. Chosen by the owner's delegation on 2026-09-12 from the
 * sweep above: about 1.6 x the ~2.2 kHz estimate, 16 dB down on the tweeter
 * branch at 2.2 kHz with the LR4's 24 dB/oct, and still inside what a 60 mm
 * cone can carry before it beams too hard. The record's own rule wants
 * >= 2 x Fs, i.e. 4400 Hz for that estimate; 3500 is below it on purpose,
 * because the tweeter's resonance is damped (no impedance peak, so little
 * excursion gain at Fs) and 4400 would waste the tweeter's best octave. It was
 * 2800 before this measurement. It stays a placeholder until a finer sweep
 * puts Fs at a point; G2 decides it with the C_SAFE interaction measured.
 *
 * subsonic 55 Hz. The product is one cabinet, a reflex alignment by the Nova's
 * own passive radiators, and its tuning waits on G0 (ADR-0021,
 * docs/04-acoustics/cabinet-plan.md). Below the passive radiators'
 * tuning the woofer unloads: the cone moves freely, excursion climbs, and no
 * sound comes out. A sealed box at least has an air spring; this does not.
 * 55 Hz is a placeholder just under the 60-65 Hz the original Nova was tuned
 * to, until the cabinet's Fb is measured. The woofer's free-air Fs measured
 * about 50 Hz on 2026-09-12 (coarse); in the box that resonance moves up, so
 * 55 Hz sits below the in-box resonance and cuts nothing the cabinet can
 * reproduce.
 *
 * gains 0.25 and 0.18, tweeter about 3 dB below the woofer. Both have come
 * down 6 dB in two steps, because the first 3 dB was not enough and the
 * amplifier still ran out near the top of the slider. The first setting distorted above about 80% on
 * the sender's volume slider, and the distortion is ANALOGUE: with these gains a
 * full-scale sample leaves the chain at 0.35, nowhere near the 0.70 limiter
 * ceiling, so nothing digital is clipping. What runs out is the amplifier. The
 * PCM5102A puts out about 2.1 Vrms at full scale and a TPA3110 at 36 dB reaches
 * full output from roughly 0.1 Vrms, so the chain is about 26 dB hotter than
 * the amplifier wants and the top of the volume range was unusable.
 *
 * A trim rather than a limiter, deliberately: a limiter would compress the
 * dynamics to fit, while a fixed attenuation moves everything down and leaves
 * the music's own shape alone. The real fix is the amplifier's gain-select
 * pins, which are still unmeasured; this buys back the top of the slider until
 * then. A 25 mm dome is
 * usually more sensitive than a small cone and there is no sensitivity
 * measurement, so the error is left on the side of less tweeter. Both are low
 * in absolute terms too, deliberately, while the amplifier's gain is unmeasured.
 *
 * These were briefly 0.35 and 0.85 to compensate a 7 uF series capacitor whose
 * corner into 4 ohm is 5.7 kHz, costing the tweeter about 7 dB at the crossover.
 * That capacitor turned out to be faulty and is not fitted, so the compensation
 * came straight back out -- with no capacitor, boosting the tweeter branch by
 * 7 dB would be pushing an UNPROTECTED driver, and the LR4 high-pass is now the
 * only thing in front of it. Put the compensation back when a good capacitor is,
 * and not before.
 *
 * ceilings 0.7 and 0.35. A tweeter takes a small fraction of a woofer's power,
 * and neither driver's power handling is known. Conservative, and the tweeter's
 * far more so. They were listened to with the amplifier on a 12 V bench
 * supply, which is what HK_BENCH_REFERENCE_SUPPLY_MV records; on the 24 V
 * adapter hk_profile_build() halves both. That halving errs quiet and promises
 * nothing about the volts at the driver -- see supply_mv_now() above for the
 * datasheet reason -- so a 24 V build of this profile logs a warning of its
 * own and is listened to staged, at low level, on one pair.
 *
 * limiter timing 150 ms release, 20 ms hold, on BOTH branches. Schema 2 lets
 * the tweeter recover on its own clock (a dome and a cone do not recover
 * alike), and G2 will set the two; until then the tweeter's numbers are the
 * woofer's, copied, so the chain behaves exactly as it did when it was
 * listened to.
 *
 * delays 0 and 0, polarity 0. The acoustic-centre offset between the cone and
 * the dome and the sign that makes their sum add at the crossover are G2's
 * sum measurement, not a bench guess: a delay chosen by ear would be a number
 * as unmeasured as Fs, and a wrong polarity is a notch at the corner. Zero is
 * "not measured", which is the truth.
 *
 * supply budget 1.0 over 100 ms. NOT A BUDGET ANYONE MEASURED. 1.0 is one
 * full-scale branch's worth of mean square -- half the validator's bound of
 * 2.0 (HK_PROFILE_SUPPLY_BUDGET_MAX), and about ten times anything these
 * gains can make -- and it is here so the stage RUNS: its detector is what
 * G1 step S7 reads off the telemetry line while raising the level into
 * 4 ohm-class loads on the adapter, and that reading replaces this number.
 * With the bench gains above and a flat EQ the summed mean square after the
 * gains cannot exceed 0.25^2 + 0.18^2 = 0.095, so 1.0 can never engage on
 * the programme alone; the user EQ sits ahead of the gains and can boost up
 * to +12 dB per band, and with enough of that the stage does engage, in its
 * only direction (quieter). The 100 ms window is a placeholder of the same
 * kind: G1 sets it from VIN's hold-up, not from here.
 *
 * amp_gain_db 0: not read. The amplifier's gain straps are bench item C3, and
 * this field is provenance for that reading; zero is what an unread strap is.
 */
static bool bench_provisional_chain(hk_profile_chain_t *out)
{
    const hk_profile_t provisional = {
        .schema             = HK_PROFILE_SCHEMA,
        .reserved           = 0,
        .measured_yyyymmdd  = 20260908u,
        .source             = "provisional-not-measured",
        .woofer_dcr_ohm     = 4.0f,    /* measured, operator record */
        .tweeter_dcr_ohm    = 3.5f,    /* operator record 2026-09-08; re-read pending, see above */
        .woofer_hpf_hz      = 55.0f,
        .crossover_hz       = 3500.0f,
        .woofer_gain        = 0.25f,
        .tweeter_gain       = 0.18f,
        .reference_supply_mv = (float)HK_BENCH_REFERENCE_SUPPLY_MV,
        .woofer_ceiling     = 0.70f,
        .tweeter_ceiling    = 0.35f,
        .woofer_release_ms  = 150u,
        .woofer_hold_ms     = 20u,
        .tweeter_release_ms = 150u,    /* the woofer's, copied; G2 sets its own */
        .tweeter_hold_ms    = 20u,
        .woofer_delay_samples  = 0u,   /* not measured (G2) */
        .tweeter_delay_samples = 0u,
        .tweeter_polarity   = 0u,      /* not measured (G2) */
        .supply_budget_sq   = 1.0f,    /* PLACEHOLDER, not a budget; G1 S7 replaces it */
        .supply_window_ms   = 100u,    /* PLACEHOLDER; G1 sets it from VIN hold-up */
        .amp_gain_db        = 0u,      /* not read (bench item C3) */
    };

    const hk_profile_verdict_t built =
        hk_profile_build(&provisional, (float)OUTPUT_RATE, supply_mv_now(), out);
    if (built != HK_PROFILE_OK) {
        ESP_LOGE(TAG, "the provisional bench profile does not even build: %s",
                 hk_profile_verdict_name(built));
        return false;
    }

    ESP_LOGW(TAG, "BENCH PROFILE IN USE AND IT WAS NOT MEASURED "
                  "(CONFIG_HK_BENCH_PROVISIONAL_PROFILE). crossover %.0f Hz, "
                  "subsonic %.0f Hz, gains %.2f/%.2f, ceilings %.2f/%.2f, "
                  "release/hold %u/%u ms on both branches, no delay, no "
                  "inversion. Only the two DC resistances are measurements; the "
                  "rest are choices derived from them. This protects a driver "
                  "by argument, not by evidence -- it is not a G0/G2 calibration "
                  "and must never be mistaken for one.",
             (double)provisional.crossover_hz, (double)provisional.woofer_hpf_hz,
             (double)provisional.woofer_gain, (double)provisional.tweeter_gain,
             (double)provisional.woofer_ceiling, (double)provisional.tweeter_ceiling,
             (unsigned int)provisional.woofer_release_ms,
             (unsigned int)provisional.woofer_hold_ms);
    ESP_LOGW(TAG, "supply budget %.2f over %u ms is a PLACEHOLDER, not the "
                  "adapter's 2.9 A: it is one full-scale branch's mean square, "
                  "far above what these gains can make from the programme, so "
                  "the stage runs but cannot engage (only under an EQ boost). "
                  "G1 step S7 replaces it with what the telemetry line reads at "
                  "the rail's sag point. Amplifier gain strapping: unread "
                  "(bench item C3).",
             (double)provisional.supply_budget_sq,
             (unsigned int)provisional.supply_window_ms);
#if CONFIG_HK_SUPPLY_MV != HK_BENCH_REFERENCE_SUPPLY_MV
    /* The bench listened at 12 V. This build is not at 12 V, so the ceilings
     * above are scaled -- and scaling is all that happens: the amplifier's
     * gain is fixed and its strapping unread, so the same slider can put up
     * to twice the bench voltage on a driver from a 24 V rail (the datasheet
     * reading is at supply_mv_now()). Said at boot, every boot, because the
     * staging rule is the only protection until C3 and G1 are read. */
    ESP_LOGW(TAG, "this profile was listened to at %u mV; this build is at "
                  "%u mV. The ceilings are scaled by min(1, %u/%u), which "
                  "errs quiet and promises nothing about driver volts: the "
                  "amplifier's gain strapping is unread (bench item C3), so the "
                  "driver may see up to twice the bench voltage at the same "
                  "slider -- staged, low level, one amplifier, one pair "
                  "(AGENTS.md).",
             (unsigned int)HK_BENCH_REFERENCE_SUPPLY_MV,
             (unsigned int)CONFIG_HK_SUPPLY_MV,
             (unsigned int)HK_BENCH_REFERENCE_SUPPLY_MV,
             (unsigned int)CONFIG_HK_SUPPLY_MV);
#endif
    return true;
}
#endif /* CONFIG_HK_BENCH_PROVISIONAL_PROFILE */

static float supply_mv_now(void)
{
    return (float)CONFIG_HK_SUPPLY_MV;
}

/** Adapter so hk_eq_settings_load() can read the user store. */
static bool read_user_u32(const char *key, uint32_t *out, void *ctx)
{
    (void)ctx;
    return hk_storage_user_read_u32(key, out);
}

/**
 * Build the protective chain from the stored calibration, or fail saying why.
 *
 * Read here rather than in hk_main because this backend owns the ::hk_dsp_t and
 * hk_dsp_init() takes the chain by value at construction: there is nowhere else
 * to put it that does not also invent a way to hand a struct across a component
 * boundary. It uses nothing but existing public API -- hk_storage's read-only
 * window onto factory_cal, and hk_profile's own judgement of what it read.
 *
 * The judgement is hk_profile_load(), and it is the same call hk_main makes
 * at boot with NULL outputs to decide whether audio is permitted at all. One
 * judge, two callers: the gate cannot permit a blob this function refuses,
 * and this function cannot build a blob the gate refused, because both
 * arguments that could differ -- the sample rate and the configured supply --
 * are the same compile-time constants in both places.
 */
static bool load_chain(hk_profile_chain_t *out)
{
    /* A blob of the wrong length is not a profile of another version, it is not
     * a profile; hk_profile_from_blob() says so, and reading into a raw buffer
     * first is what lets it check the length instead of trusting it. */
    uint8_t raw[sizeof(hk_profile_t)];
    size_t  length = sizeof(raw);
    if (hk_storage_factory_get_blob(HK_STORAGE_PROFILE_KEY, raw, &length) != ESP_OK) {
        ESP_LOGW(TAG, "no calibration profile in %s/%s",
                 HK_STORAGE_FACTORY_PARTITION, HK_STORAGE_PROFILE_KEY);
#if CONFIG_HK_BENCH_PROVISIONAL_PROFILE
        return bench_provisional_chain(out);
#else
        return false;
#endif
    }

    hk_profile_t               profile;
    const hk_profile_verdict_t verdict = hk_profile_load(raw, length, (float)OUTPUT_RATE,
                                                         supply_mv_now(), &profile, out);
    if (verdict != HK_PROFILE_OK) {
        /* Both outputs are zeroed by the callee on any refusal, so `out` holds
         * nothing half-built for the caller to run on by mistake. */
        ESP_LOGE(TAG, "stored calibration refused (%s) at %u Hz, %u mV; the boot "
                      "gate reached the same verdict and audio stays muted",
                 hk_profile_verdict_name(verdict), (unsigned int)OUTPUT_RATE,
                 (unsigned int)CONFIG_HK_SUPPLY_MV);
        return false;
    }

    ESP_LOGI(TAG, "calibration %u from '%s': crossover %.0f Hz, subsonic %.0f Hz, "
                  "release/hold %u/%u and %u/%u ms, delay %u/%u samples, tweeter %s, "
                  "supply budget %.3f over %u ms, amplifier gain %s",
             (unsigned int)profile.measured_yyyymmdd, profile.source,
             (double)profile.crossover_hz, (double)profile.woofer_hpf_hz,
             (unsigned int)profile.woofer_release_ms, (unsigned int)profile.woofer_hold_ms,
             (unsigned int)profile.tweeter_release_ms, (unsigned int)profile.tweeter_hold_ms,
             (unsigned int)profile.woofer_delay_samples,
             (unsigned int)profile.tweeter_delay_samples,
             profile.tweeter_polarity ? "inverted" : "in phase",
             (double)profile.supply_budget_sq, (unsigned int)profile.supply_window_ms,
             profile.amp_gain_db ? "read" : "unread (bench item C3)");
    if (profile.amp_gain_db) {
        ESP_LOGI(TAG, "amplifier gain strapping recorded as %u dB; carried, not "
                      "computed with",
                 (unsigned int)profile.amp_gain_db);
    }
    return true;
}

/**
 * Bring the signal path up.
 *
 * Both halves are read here, and they are read from different places on
 * purpose: the protective numbers come from `factory_cal`, which a user reset
 * cannot reach, and the tonal ones from the user store, which it can. That is
 * PRD-008, and hk_dsp.h calls the struct boundary between them the same wall
 * one layer up. Nothing in this function can move a value across it.
 */
static bool dsp_start(void)
{
    hk_profile_chain_t chain;
    const bool         calibrated = load_chain(&chain);

    /* Tonal settings never stop the speaker: a row that is missing or out of
     * range falls back to its default and hk_eq_settings_load() reports that as
     * diagnostic information, not as a failure. */
    hk_eq_settings_t eq;
    if (!hk_eq_settings_load(&eq, read_user_u32, NULL)) {
        ESP_LOGI(TAG, "some EQ settings are not stored; defaults used for those");
    }

    /* NULL chain is the documented way to say "uncalibrated" -- a refusal
     * rather than a reason to invent defaults. */
    return hk_dsp_init(&s_dsp, calibrated ? &chain : NULL, &eq, (float)OUTPUT_RATE);
}

/* ==========================================================================
 * Output state, copied from the vendored backend
 * ========================================================================== */

static i2s_chan_handle_t tx_handle;
static volatile bool     flush_requested = false;
static volatile bool     playback_running = false;
static TaskHandle_t      playback_task_handle = NULL;
static volatile int      source_rate = 44100;
static volatile bool     resample_reinit_needed = false;

/* Live output cursor. Copied from vendor/audio/audio_output.c:76-120 together
 * with its comment, because the reasoning is the part that matters:
 *
 *   output_submitted_frames advances after a successful i2s_channel_write();
 *   output_sent_frames is advanced by the TX DMA completion ISR. Their
 *   difference is the amount of audio queued ahead of the next write, i.e. the
 *   real pipeline delay.
 *
 *   auto_clear keeps the DMA clocking descriptors even when the writer stalls,
 *   so sent can overtake submitted. The excess is output time that was played
 *   as silence and can never be recovered; it is folded into
 *   output_lost_frames so that the queue depth stays non-negative and the
 *   cursor keeps a stable meaning across a starvation episode. */
static uint64_t output_submitted_frames;
static uint64_t output_sent_frames;
static uint64_t output_lost_frames;
static uint32_t output_underruns;

static bool IRAM_ATTR audio_output_on_sent(i2s_chan_handle_t handle,
                                           i2s_event_data_t *event,
                                           void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    if (event && event->size > 0) {
        __atomic_add_fetch(&output_sent_frames,
                           (uint64_t)(event->size / (2U * sizeof(int16_t))),
                           __ATOMIC_RELAXED);
    }
    return false;
}

static void output_cursor_reset(void)
{
    __atomic_store_n(&output_submitted_frames, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&output_sent_frames, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&output_lost_frames, 0, __ATOMIC_RELAXED);
}

/* Frames queued in the DMA ring ahead of the next write. Called from the
 * playback task only, which is also the sole writer of the submitted and lost
 * counters, so the rebase below needs no lock.
 * Copied from vendor/audio/audio_output.c:103-120. */
static uint32_t output_queued_frames(void)
{
    uint64_t submitted = __atomic_load_n(&output_submitted_frames, __ATOMIC_RELAXED);
    uint64_t lost      = __atomic_load_n(&output_lost_frames, __ATOMIC_RELAXED);
    uint64_t sent      = __atomic_load_n(&output_sent_frames, __ATOMIC_RELAXED);

    if (sent > submitted + lost) {
        /* The ring ran dry: rebase so queued reads 0 and remember how much
         * output time went out as silence. */
        __atomic_store_n(&output_lost_frames, sent - submitted, __ATOMIC_RELAXED);
        output_underruns++;
        return 0;
    }

    uint64_t       queued = submitted + lost - sent;
    const uint64_t ring   = (uint64_t)I2S_DMA_DESC_NUM * I2S_DMA_FRAME_NUM;
    return queued > ring ? (uint32_t)ring : (uint32_t)queued;
}

/*
 * The sender's volume, ramped. Copied from vendor/audio/audio_output.c:144-171
 * verbatim, comment included, because it is the same problem with the same
 * answer and a second implementation would only be a second thing to keep in
 * step:
 *
 *   Ramp toward the target gain instead of applying volume changes instantly.
 *   An abrupt gain step mid-waveform is a discontinuity scaled by the signal's
 *   current amplitude -- the classic volume "zipper" click, audible on every
 *   step of the sender's volume slider. Approach the target exponentially,
 *   stepping once per stereo frame (even indices) so both channels always carry
 *   the same gain; the /256 divisor gives a ~3 ms time constant and a
 *   worst-case per-frame gain step of ~0.4%, with a minimum step of 1 so the
 *   ramp always completes.
 *
 * It runs BEFORE the DSP, which is where the vendored file runs it and where it
 * belongs: the limiter ceilings from G2 are an absolute cap on what reaches the
 * amplifier, so the volume control has to sit upstream of them. A volume
 * applied after the limiter would scale the ceiling along with the audio and
 * protect nothing at full volume.
 *
 * It is also the one downstream operation hk_dsp.h explicitly permits, but only
 * because it is upstream: "a common gain applied to both slots equally ... lands
 * after the limiters, so it can only make the output quieter". Here it lands
 * before them, which is stricter still.
 */
static void apply_volume(int16_t *buf, size_t n)
{
#ifndef CONFIG_DAC_CONTROLS_VOLUME
    static int32_t cur_q15 = -1;
    int32_t        target  = airplay_get_volume_q15();
    if (cur_q15 < 0) {
        cur_q15 = target; /* first call: no audio has played yet, jump silently */
    }
    for (size_t i = 0; i < n; i++) {
        if ((i & 1) == 0 && cur_q15 != target) {
            int32_t diff = target - cur_q15;
            int32_t step = diff / 256;
            if (step == 0) {
                step = diff > 0 ? 1 : -1;
            }
            cur_q15 += step;
        }
        buf[i] = (int16_t)(((int32_t)buf[i] * cur_q15) >> 15);
    }
#else
    (void)buf;
    (void)n;
#endif
}

/* ==========================================================================
 * DSP telemetry -- ours; the vendored file has nothing to measure here
 * ========================================================================== */

/**
 * How long the chain takes, and what the supply detector saw.
 *
 * Two numbers the record asks for and nowhere holds. The firmware plan's F3
 * acceptance wants the DSP's task time at 240 MHz as a MEASURED figure, and
 * G1 step S7 needs the supply detector's mean square at the point where the
 * adapter's rail sags, to write into the profile as supply_budget_sq.
 * Neither exists on paper: the first depends on the compiler and the cache,
 * the second on the amplifier's gain strapping and on the load. So the
 * playback task brackets hk_dsp_process() with esp_timer_get_time(), keeps
 * the worst and the mean block time over an interval of PLAYED programme,
 * tracks the detector's maximum and the common gain's minimum over the same
 * interval, and logs one line per interval:
 *
 *   dsp block max M us mean m us of D us (...); supply mean_sq max S gain min g
 *
 * M and m against D is the deadline question the F3 criterion asks. S is the
 * number G1 reads while raising the level; g says whether the supply stage
 * engaged at all (1.000 means it never did). Both are read from the DSP the
 * playback task owns, after the block it just ran -- the same task, so there
 * is nothing to race.
 *
 * Programme only. Silence frames run the chain too (so the filters ring out),
 * and only part of their cost is the same per frame: the supply stage's
 * per-sample cost is data-independent (one square root per block), but the
 * two peak limiters branch on the signal and divide while engaged
 * (hk_limiter.c), which is why the line reports both the worst block and the
 * mean rather than one number. What excludes silence is the detector: on
 * silence it decays to zero, and a mean over an idle receiver says nothing
 * about a playing one. The interval is counted in output frames, not wall
 * time, so "60 s of playback" (10 s on the bench build) means what it says.
 *
 * D is the duration of a FRAME_SAMPLES block, and M and m are NORMALISED to
 * that block before they are kept: a resampled source (48 kHz in) yields
 * shorter blocks -- about 323 output frames instead of 352 -- and each
 * block's measured time is scaled by FRAME_SAMPLES / frames before it enters
 * the maximum and the sum, so M and m are always "per 352-frame block" and D
 * is always the deadline to read them against. Without that, a 48 kHz stream
 * whose 323-frame blocks took 7500 us would have printed as met against
 * 7981 us when a 323-frame block's own deadline is 7324 us. The line prints
 * the blocks' observed mean length beside the block the figures are
 * normalised to, so the scaling is visible rather than assumed. A block of
 * zero output frames (the resampler can return one while it buffers) timed
 * nothing and is not counted.
 *
 * The block time is the chain's alone: the volume ramp, the LED feed, the
 * resampler and the I2S write are all outside the brackets. Two clock reads
 * per block at 125 Hz is cheap enough not to disturb what it measures.
 *
 * Nothing here is an operator measurement until an operator records the line
 * in docs/06-testing/ against a named build and a named stream; until then it
 * is instrumentation, and the F3 figure is still open.
 */
typedef struct {
    uint32_t frames;      /**< Programme frames in the interval so far */
    uint32_t blocks;      /**< Programme blocks timed */
    uint64_t sum_us;      /**< Sum of block times, for the mean */
    uint32_t max_us;      /**< Worst block */
    float    mean_sq_max; /**< The supply detector's highest reading */
    float    gain_min;    /**< The supply stage's lowest common gain */
} dsp_telemetry_t;

#if CONFIG_HK_BENCH_PROVISIONAL_PROFILE
/* G1 reads the detector while stepping the level up, and a minute per
 * reading is an afternoon per curve. Bench build only. */
#define DSP_TELEMETRY_INTERVAL_S 10u
#else
#define DSP_TELEMETRY_INTERVAL_S 60u
#endif
#define DSP_TELEMETRY_INTERVAL_FRAMES ((uint32_t)OUTPUT_RATE * DSP_TELEMETRY_INTERVAL_S)

/** The deadline: one FRAME_SAMPLES block's duration at the output rate. */
#define DSP_BLOCK_PERIOD_US \
    ((uint32_t)(((uint64_t)FRAME_SAMPLES * 1000000ULL) / (uint64_t)OUTPUT_RATE))

static void dsp_telemetry_reset(dsp_telemetry_t *t)
{
    *t = (dsp_telemetry_t){ .gain_min = 1.0f };
}

static void dsp_telemetry_note(dsp_telemetry_t *t, int64_t block_us, size_t frames)
{
    /* Nothing to normalise a zero-frame block to, and nothing it measured:
     * the resampler can hand back no output frames while it buffers. */
    if (frames == 0u) {
        return;
    }

    /* Normalised to a FRAME_SAMPLES block before it is kept, so the maximum
     * and the mean are always "per 352-frame block" and DSP_BLOCK_PERIOD_US
     * is always their deadline, whatever the source rate made of this block's
     * length. Integer, in 64 bits: the product overflows only past 1600 years
     * of block time. The clock is monotonic and a block is milliseconds, so
     * neither clamp should ever act; they are here so a wrapped or negative
     * reading cannot become a nonsense maximum. */
    uint64_t took_us = (block_us < 0) ? 0u : (uint64_t)block_us;
    took_us          = (took_us * (uint64_t)FRAME_SAMPLES) / (uint64_t)frames;
    const uint32_t took =
        (took_us > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)took_us;

    t->frames += (uint32_t)frames;
    t->blocks += 1u;
    t->sum_us += took;
    if (took > t->max_us) {
        t->max_us = took;
    }
    if (s_dsp.supply_limit.mean_sq > t->mean_sq_max) {
        t->mean_sq_max = s_dsp.supply_limit.mean_sq;
    }
    if (s_dsp.supply_limit.gain < t->gain_min) {
        t->gain_min = s_dsp.supply_limit.gain;
    }

    if (t->frames < DSP_TELEMETRY_INTERVAL_FRAMES) {
        return;
    }
    ESP_LOGI(TAG, "dsp block max %u us mean %u us of %u us (per %u-frame block, "
                  "normalised; %u blocks of %u frames on average at %u Hz, "
                  "%u biquads); supply mean_sq max %.4f gain min %.3f",
             (unsigned int)t->max_us,
             (unsigned int)(t->sum_us / t->blocks),
             (unsigned int)DSP_BLOCK_PERIOD_US,
             (unsigned int)FRAME_SAMPLES,
             (unsigned int)t->blocks,
             (unsigned int)(t->frames / t->blocks),
             (unsigned int)OUTPUT_RATE,
             (unsigned int)hk_dsp_biquads_per_frame(&s_dsp),
             (double)t->mean_sq_max, (double)t->gain_min);
    dsp_telemetry_reset(t);
}

/* --- The playback task ----------------------------------------------------
 *
 * Structurally the vendored playback_task (vendor/audio/audio_output.c:201-267)
 * with four changes, the first three of them the point of the file:
 *
 *   1. The silence buffer is re-zeroed on every frame that uses it. Upstream
 *      zeroes it once at allocation and then relies on nobody writing to it,
 *      which stops being true the moment a processing stage is added.
 *
 *   2. Every buffer handed to the DSP was completely written earlier in the
 *      same iteration by something that is not the DSP, so in-place processing
 *      here cannot read back a previous output.
 *
 *   3. The DSP call sits where apply_channel_mode() used to. There is no
 *      channel selection anywhere: upstream picks a channel because its two
 *      outputs are a left speaker and a right speaker, and ours are a woofer
 *      band and a tweeter band (ADR-0002). The DSP's stage 1 sums the pair,
 *      and mono is the only programme this product has (ADR-0021).
 *
 *   4. That call is bracketed by the clock, for the telemetry line above.
 *      Upstream has nothing to time there; this file has a chain whose cost
 *      the record wants as a measured number.
 */
static void playback_task(void *arg)
{
    (void)arg;

    int16_t *pcm          = malloc((size_t)(FRAME_SAMPLES + 1) * 2 * sizeof(int16_t));
    int16_t *resample_buf = malloc(MAX_RESAMPLE_FRAMES * 2 * sizeof(int16_t));
    int16_t *silence      = malloc((size_t)FRAME_SAMPLES * 2 * sizeof(int16_t));

    if (!pcm || !resample_buf || !silence) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(pcm);
        free(resample_buf);
        free(silence);
        playback_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    const size_t silence_bytes = (size_t)FRAME_SAMPLES * 2 * sizeof(int16_t);

    /* Said once rather than per frame: a log line in the playback loop is a log
     * line at 125 Hz. */
    bool announced_refusal = false;

    /* Ours: the block-time and supply-detector line, once per interval of
     * played programme. Task-local, because the task is its only writer. */
    dsp_telemetry_t telemetry;
    dsp_telemetry_reset(&telemetry);

    size_t written;
    while (playback_running) {
        if (resample_reinit_needed) {
            resample_reinit_needed = false;
            audio_resample_init((uint32_t)source_rate, OUTPUT_RATE, 2);
            /* Ours, and for the same reason as the flush below rather than a
             * different one -- hk_dsp.h names both cases in one sentence: reset
             * is "for a stream flush OR A RATE CHANGE, where the samples that
             * follow have no relationship to the ones before".
             *
             * A rate change arrives from audio_receiver_set_format(), which
             * rtsp_handlers.c calls on SETUP and ANNOUNCE (lines 1005, 1091,
             * 1219) -- and none of those three is accompanied by an
             * audio_output_flush(), so the branch below does NOT cover this
             * one. Without this call the previous stream's filter memory and
             * the limiters' reduced gain ring out over the first frames of the
             * new one. Measured on the host, on the chain as it then was (70 Hz
             * second-order subsonic, 4 kHz LR4; the subsonic filter has since
             * become fourth order and the figures were not re-taken), by
             * feeding an all-zero frame into a path left by a loud 300 Hz
             * passage: the retained state alone produces a woofer peak of 8598
             * and a TWEETER peak of 5268 (-15.9 dBFS) out of digital silence,
             * and the tweeter branch's ceiling does not prevent it -- a limiter
             * caps a level, it does not remove a discontinuity. With the reset
             * both peaks are 0. The delay rings and the supply detector are
             * state of the same kind and the same reset clears them.
             *
             * The resampler is already re-initialised on the line above; this
             * is the same statement about the stage after it. */
            hk_dsp_reset(&s_dsp);
        }
        if (flush_requested) {
            flush_requested = false;
            audio_resample_reset();
            /* Ours. The filters and limiters carry state across the
             * discontinuity too, and the tail of audio that was just discarded
             * would otherwise ring out over the first frames of the new
             * stream. */
            hk_dsp_reset(&s_dsp);
            i2s_channel_disable(tx_handle);
            output_cursor_reset();
            i2s_channel_enable(tx_handle);
        }

        /* Read unconditionally, even when the DSP cannot play what is read. The
         * receiver's jitter buffer has to keep draining or the timing engine
         * sees a pipeline that never empties, and the RTSP session that is
         * still perfectly alive backs up behind a silent output stage. */
        size_t samples = audio_receiver_read(pcm, FRAME_SAMPLES + 1);

        int16_t *play_buf;
        size_t   play_samples;

        if (samples > 0 && hk_dsp_ready(&s_dsp)) {
            play_buf     = pcm;
            play_samples = samples;
            if (audio_resample_is_active()) {
                play_samples = audio_resample_process(pcm, samples, resample_buf,
                                                      MAX_RESAMPLE_FRAMES);
                play_buf     = resample_buf;
            }
            apply_volume(play_buf, play_samples * 2);

            /* Upstream feeds the buffer it is about to write; this feeds the
             * PROGRAMME, one line earlier, because after the next call the
             * buffer holds two crossover ways rather than a stereo image and a
             * level meter fed from it would show woofer-band energy on the left
             * and tweeter-band on the right instead of the loudness of the
             * track. Academic today -- shim/hk_airplay_shim.c drops these
             * samples, because hk_ui owns the LED -- but the next person needs
             * the argument, not the outcome. */
            led_audio_feed(play_buf, play_samples);
        } else {
            /* Two cases arrive here and both want the same buffer: a receiver
             * underflow, and a DSP that has no chain to run.
             *
             * THE MEMSET IS THE POINT OF THIS FILE. hk_dsp_process() writes in
             * place, so without it frame N's output would be frame N+1's input
             * and an EQ boost would make the "silence" grow. Unconditional, and
             * cheap: 1408 bytes at 125 Hz. */
            memset(silence, 0, silence_bytes);
            play_buf     = silence;
            play_samples = FRAME_SAMPLES;
            led_audio_feed(silence, FRAME_SAMPLES);
        }

        /* Where apply_channel_mode() used to be.
         *
         * Called even for the silence frame, and deliberately: it lets the
         * filters and the limiters ring out and recover across a gap instead of
         * freezing mid-decay, so the first frame after an underrun continues
         * the tail rather than stepping off it. When the path is not ready it
         * zeroes the buffer and says no, which is why the buffer written below
         * is safe in every branch.
         *
         * Bracketed by the clock for the telemetry line (dsp_telemetry_note);
         * the brackets hold the chain and nothing else. */
        const int64_t dsp_began_us = esp_timer_get_time();
        const bool    processed    = hk_dsp_process(&s_dsp, play_buf, play_samples);
        const int64_t dsp_took_us  = esp_timer_get_time() - dsp_began_us;

        if (!processed) {
            if (!announced_refusal) {
                announced_refusal = true;
                ESP_LOGW(TAG, "no DSP chain (%s): writing digital zero. The "
                              "receiver runs and I2S is clocked, but nothing is "
                              "audible until a calibration profile exists "
                              "(G0/G2). The DAC should already be muted by the "
                              "same gate; the amplifiers have no mute of their "
                              "own.",
                         hk_dsp_refusal_name(hk_dsp_refusal(&s_dsp)));
            }
        } else {
            if (announced_refusal) {
                announced_refusal = false;
                ESP_LOGI(TAG, "DSP chain available; audio is being processed again");
            }
            if (play_buf != silence) {
                dsp_telemetry_note(&telemetry, dsp_took_us, play_samples);
            }
        }

#if CONFIG_HK_OUTPUT_SWAP_BRANCHES
        /* The wiring, not the design.
         *
         * hk_dsp puts the woofer branch in the left slot because ADR-0002 and
         * the wiring plan both say the left analogue channel drives the woofer.
         * This undoes that for a board where the two are physically the other
         * way round, and it belongs here rather than inside the DSP: the DSP
         * describes the loudspeaker, this line describes one bench's solder.
         *
         * It is deliberately AFTER the limiter, because it is a relabelling and
         * not a processing stage. Nothing about the samples changes -- the
         * high-passed, limited tweeter band is still exactly that -- only which
         * pin it leaves on. */
        for (size_t i = 0; i < play_samples; i++) {
            const int16_t held = play_buf[2u * i];
            play_buf[2u * i] = play_buf[2u * i + 1u];
            play_buf[2u * i + 1u] = held;
        }
#endif

        if (i2s_channel_write(tx_handle, play_buf,
                              play_samples * 2 * sizeof(int16_t), &written,
                              portMAX_DELAY) == ESP_OK) {
            __atomic_add_fetch(&output_submitted_frames,
                               (uint64_t)(written / (2U * sizeof(int16_t))),
                               __ATOMIC_RELAXED);
        }

        /* Upstream yields after a real frame and lets the blocking write pace
         * the silence one (vendor/audio/audio_output.c:247,250-251): "block on
         * the DMA write (portMAX_DELAY) so the write itself paces the loop,
         * instead of a short timeout plus vTaskDelay(1) which produced jittery
         * silence". Both paths end in the same blocking write here.
         *
         * The condition is NOT upstream's, and the difference is worth naming
         * rather than glossing: upstream yields whenever the receiver returned
         * samples, this yields whenever those samples were PLAYED. They part
         * company in one state -- receiver delivering, DSP not ready -- which
         * is the whole of an uncalibrated device's life, and there this does
         * not yield where upstream would. Harmless, and checked rather than
         * assumed: the i2s_channel_write() above is portMAX_DELAY, so the loop
         * still blocks once per frame and cannot starve a lower-priority task.
         * The yield is a courtesy on top of that, not the thing that provides
         * it. Tying it to play_buf keeps it describing the branch it is in. */
        if (play_buf != silence) {
            taskYIELD();
        }
    }

    free(pcm);
    free(resample_buf);
    free(silence);
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}

/* --- The entry points the compiled vendored sources actually call ----------
 *
 * Established by grepping the sources this component compiles, not by reading
 * audio_output.h: the header declares more than any one backend has to supply,
 * audio_output_common.c carries weak defaults for the optional half, and
 * upstream files this project does not compile (web_server.c, the A2DP path)
 * call the rest.
 *
 *   audio_output_init                 hk_airplay.c:238
 *   audio_output_start                hk_airplay.c:249
 *   audio_output_flush                vendor/rtsp/rtsp_server.c:272,
 *                                     vendor/rtsp/rtsp_handlers.c:1714,1729,
 *                                       1782,1820,1905
 *   audio_output_set_source_rate      vendor/audio/audio_receiver.c:152
 *   audio_output_get_hardware_latency_us
 *                                     vendor/audio/audio_timing.c:213,321,330,334
 *   audio_output_get_pipeline_us      vendor/audio/audio_timing.c:211   (weak default exists)
 *   audio_output_get_underruns        vendor/audio/audio_timing.c:849   (weak default exists)
 *   audio_output_set_channel_mode     hk_airplay.c                      (weak default exists)
 *
 * The first five have no weak default and MUST be defined here. The next two
 * are defined because this backend genuinely has a completion cursor and a real
 * underrun count, and letting the weak "I cannot answer" default stand would
 * throw away the measurement the timing engine prefers.
 *
 * audio_output_set_channel_mode is defined for a reason worth stating, because
 * the weak default would have compiled and linked and quietly done nothing --
 * and its companion audio_output_get_channel_mode() would then report STEREO,
 * the enum's zero value, for a box that has no stereo. This backend has
 * nothing to select: the DSP's stage 1 sums the pair and mono is the only
 * programme (ADR-0021). So the setter accepts the call hk_airplay.c makes,
 * clamps everything to MONO, and the getter answers MONO, so nothing can read
 * back a mode this backend is not in. Cycle and locked are left to the weak
 * defaults: no compiled source calls them.
 *
 * audio_output_write() is deliberately NOT defined, and that is a safety
 * decision rather than a shortcut. It exists upstream so another source (A2DP)
 * can push raw PCM straight into I2S while the AirPlay task is stopped -- which
 * on this product means straight past the crossover, the subsonic filter and
 * both limiters, into an amplifier wired to an unmeasured tweeter. No compiled
 * source calls it, so its absence costs nothing today and a future caller gets
 * a link error instead of an unprotected path.
 *
 * audio_output_set_sample_rate() is not defined for the same reason: it exists
 * for a Bluetooth path this firmware does not have, and nothing compiled calls
 * it.
 */

esp_err_t audio_output_init(void)
{
    /* Copied from vendor/audio/audio_output.c:286-353, minus the channel-mode
     * restore from upstream's own NVS (there is no channel mode to restore,
     * see audio_output_set_channel_mode) and plus the DSP bring-up. */
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = I2S_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = I2S_DMA_FRAME_NUM;
    /* Zero each DMA descriptor after it is sent. Without this, a writer stall
     * longer than the DMA ring (~46 ms -- e.g. an NVS/flash write disabling the
     * cache, or a CPU burst) makes the hardware REPLAY the stale ring contents
     * in a loop: a loud stutter, then a second discontinuity on recovery. With
     * auto_clear an underrun degrades to plain silence. Upstream's reasoning
     * and upstream's setting; it matters more here, because the stale ring
     * would be replaying a limited tweeter branch on repeat. */
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_handle, NULL), TAG,
                        "channel create failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(OUTPUT_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = I2S_SCK_PIN,
                .bclk = I2S_BCK_PIN,
                .ws   = I2S_LRCK_PIN,
                .dout = I2S_DOUT_PIN,
                .din  = I2S_GPIO_UNUSED,
            },
    };
#ifdef I2S_GND_PIN
    gpio_reset_pin(I2S_GND_PIN);
    gpio_set_direction(I2S_GND_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_GND_PIN, 0);
#endif
#ifdef I2S_VCC_PIN
    gpio_reset_pin(I2S_VCC_PIN);
    gpio_set_direction(I2S_VCC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_VCC_PIN, 1);
#endif

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_handle, &std_cfg), TAG,
                        "std mode init failed");

    /* TX completion callback drives the live output cursor used by the timing
     * engine (see audio_output_get_pipeline_us). */
    const i2s_event_callbacks_t callbacks = {
        .on_recv       = NULL,
        .on_recv_q_ovf = NULL,
        .on_sent       = audio_output_on_sent,
        .on_send_q_ovf = NULL,
    };
    ESP_RETURN_ON_ERROR(
        i2s_channel_register_event_callback(tx_handle, &callbacks, NULL), TAG,
        "event callback registration failed");
    output_cursor_reset();

    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_handle), TAG, "channel enable failed");
    ESP_LOGI(TAG, "I2S initialized: Rate=%u, DMA_Desc=%d, DMA_Frame=%d",
             (unsigned int)OUTPUT_RATE, I2S_DMA_DESC_NUM, I2S_DMA_FRAME_NUM);

    /* MCLK/BCLK/LRCK are now running. Some codecs need this edge to finish
     * their clock setup. The PCM5102A of ADR-0002 has no control interface and
     * no driver is registered, so this is a no-op here -- kept because dropping
     * it would be a silent divergence from the file this shadows. */
    dac_on_i2s_started();

    audio_resample_init(44100, OUTPUT_RATE, 2);

    /* Ours. A refusal is not an init failure: the receiver, the clock and the
     * network side are all perfectly able to run, and returning an error here
     * would take AirPlay discovery down over a missing calibration file. The
     * playback task writes zero instead, and says so once. */
    if (!dsp_start()) {
        ESP_LOGW(TAG, "DSP path refused (%s). Output will be digital zero until "
                      "a calibration profile exists (G0/G2).",
                 hk_dsp_refusal_name(hk_dsp_refusal(&s_dsp)));
    } else {
        /* The biquad count comes from the chain, not from a literal here, so
         * this line cannot drift from what hk_dsp actually runs. */
        ESP_LOGI(TAG, "DSP path built at %u Hz: %u biquads per frame "
                      "(protective plus whatever EQ is on), alignment delay "
                      "%u/%u samples, tweeter %s, supply-budget stage, two peak "
                      "limiters. "
                      "Left = WOOFER, right = TWEETER (ADR-0002); this is not a "
                      "stereo pair. Block time is logged every %u s of playback.",
                 (unsigned int)OUTPUT_RATE,
                 (unsigned int)hk_dsp_biquads_per_frame(&s_dsp),
                 (unsigned int)s_dsp.chain.woofer_delay_samples,
                 (unsigned int)s_dsp.chain.tweeter_delay_samples,
                 (s_dsp.chain.tweeter_sign < 0.0f) ? "inverted" : "in phase",
                 (unsigned int)DSP_TELEMETRY_INTERVAL_S);
    }

    return ESP_OK;
}

void audio_output_start(void)
{
    /* Copied from vendor/audio/audio_output.c:356-368, including the task
     * priority (AUDIO_PLAYBACK_TASK_PRIORITY, which audio_output.h explains
     * must outrank every source task) and the core pinning.
     *
     * The stack is upstream's 4096 unchanged. The DSP adds no allocation and no
     * recursion to the frame -- its state is the caller-owned hk_dsp_t in
     * static storage, and its working set is a handful of floats. */
    if (playback_task_handle != NULL) {
        return; /* already running */
    }
    playback_running = true;
    /* The DMA has been free-running since the last session, so the cursor
     * carries an arbitrary submitted/sent skew. Start from a clean slate. */
    output_cursor_reset();
    /* Ours, and the same sentence as the line above applied to the signal path:
     * the filters and the limiters also carry the last session's state, and a
     * session that ended mid-passage leaves a decaying tail and a limiter gain
     * still held down. Safe to touch s_dsp here because this function returned
     * early if a playback task exists, so there is no other reader.
     *
     * audio_output_stop() has no compiled caller today, which makes this
     * unreachable rather than wrong -- and that is exactly why it belongs here.
     * stop() is defined in this file on the argument that "a start with no stop
     * leaves the task and its buffers alive with no way to reclaim them"; the
     * day something calls it, the restart must not resume on the old stream's
     * filter memory. Costs the filter states, two 64-float delay rings, two
     * limiter re-inits and the supply detector, once. */
    hk_dsp_reset(&s_dsp);
    xTaskCreatePinnedToCore(playback_task, "audio_play", 4096, NULL,
                            AUDIO_PLAYBACK_TASK_PRIORITY, &playback_task_handle,
                            PLAYBACK_CORE);
}

void audio_output_stop(void)
{
    /* Copied from vendor/audio/audio_output.c:370-385. Nothing this component
     * compiles calls it today; defined anyway, because audio_output_start()
     * exists and a start with no stop leaves the task and its buffers alive
     * with no way to reclaim them. */
    if (playback_task_handle == NULL) {
        return;
    }
    playback_running = false;
    int timeout      = 40;
    while (playback_task_handle != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (playback_task_handle != NULL) {
        ESP_LOGW(TAG, "Playback task did not exit within timeout");
    } else {
        ESP_LOGI(TAG, "Playback task stopped");
    }
}

void audio_output_flush(void)
{
    flush_requested = true;
}

void audio_output_set_source_rate(int rate)
{
    if (rate > 0 && rate != source_rate) {
        source_rate            = rate;
        resample_reinit_needed = true;
    }
}

uint32_t audio_output_get_hardware_latency_us(void)
{
    /* Copied from vendor/audio/audio_output.c:416-435 with its reasoning, which
     * holds here because the DMA geometry is identical:
     *
     *   Delay between i2s_channel_write() accepting a sample and that sample
     *   leaving the DAC. This is the DMA ring occupancy AHEAD of the newly
     *   written data, which is NOT the full ring: i2s_channel_write() blocks
     *   only until space frees, so the writer refills as soon as a descriptor
     *   completes and steady-state occupancy oscillates between (DESC_NUM - 1)
     *   and DESC_NUM descriptors.
     *
     *   Using the full ring overstates the delay by half a descriptor on
     *   average -- 2.9 ms at 44.1 kHz -- and that bias lands directly in
     *   compute_early_us(), pushing every frame toward the "late" side of the
     *   threshold. Model the midpoint instead:
     *     (DESC_NUM - 0.5) x FRAME_NUM == (2*DESC_NUM - 1) x FRAME_NUM / 2
     *   The residual +/-2.9 ms swing is real jitter that the drift servo in
     *   audio_timing.c absorbs; only the constant bias is removed here.
     *
     * The DSP adds no samples to the pipeline this models. hk_dsp.h states
     * it: no frame waits for a later one -- every stage is a recursive filter
     * or a memoryless multiply, the peak limiter has no lookahead by
     * deliberate design (hk_limiter.h), and the supply stage decides its gain
     * from samples it has already seen. The one stage that buffers, the
     * alignment delay, delays ONE branch relative to the other by at most 64
     * samples (1.45 ms here) and the undelayed branch still leaves in the
     * frame it arrived in, so the pipeline depth is unchanged and this number
     * stays true; the offset is inside the +/-2.9 ms swing the paragraph above
     * already hands to the drift servo. */
    return (uint32_t)((((uint64_t)(2 * I2S_DMA_DESC_NUM - 1) * I2S_DMA_FRAME_NUM *
                        1000000ULL) /
                       2) /
                      OUTPUT_RATE);
}

bool audio_output_get_pipeline_us(int64_t *now_us, uint32_t *pipeline_us)
{
    /* Copied from vendor/audio/audio_output.c:437-451. Sample the queue depth
     * first, then the clock: any DMA completion that lands between the two
     * makes the reported depth slightly stale in the conservative direction (we
     * believe the pipeline is fuller, i.e. that the next sample plays later,
     * than it really is). The error is bounded by one descriptor period and is
     * absorbed by the position servo. */
    uint32_t queued = output_queued_frames();
    if (now_us) {
        *now_us = esp_timer_get_time();
    }
    if (pipeline_us) {
        *pipeline_us = (uint32_t)(((uint64_t)queued * 1000000ULL) / OUTPUT_RATE);
    }
    return true;
}

uint32_t audio_output_get_underruns(void)
{
    return __atomic_load_n(&output_underruns, __ATOMIC_RELAXED);
}

void audio_output_set_channel_mode(audio_channel_mode_t mode)
{
    /* Not persisted, unlike vendor/audio/audio_output.c:496-511, and not
     * stored anywhere at all: there is one mode. Upstream picks a channel or
     * downmixes because its two outputs are a left speaker and a right
     * speaker; ours are a woofer band and a tweeter band (ADR-0002), and the
     * DSP's stage 1 already sums the pair. Any other request is clamped to
     * MONO and said so, rather than silently honoured into a driver.
     *
     * Nor is it gated on audio_output_channel_mode_locked(): that guard exists
     * upstream for boards with two DACs, where the hardware already fixes the
     * routing. This board has one DAC and the routing is the crossover. */
    if (mode != AUDIO_CHANNEL_MONO) {
        ESP_LOGW(TAG, "channel mode %d requested; this product has one programme, "
                      "mono, and plays that",
                 (int)mode);
    }
    ESP_LOGI(TAG, "programme: MONO (L+R)/2");
}

audio_channel_mode_t audio_output_get_channel_mode(void)
{
    /* Answered rather than left to the weak default, which would report
     * STEREO -- the enum's zero value -- for a box that has none. */
    return AUDIO_CHANNEL_MONO;
}
