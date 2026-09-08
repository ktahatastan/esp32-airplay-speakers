/**
 * @file hk_airplay_output_i2s.c
 * @brief The third output backend: I2S to the PCM5102A, through this project's
 *        crossover, protective filters and limiter.
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
 * Process in place there and the buffer stops being silence: frame N's output
 * becomes frame N+1's input. The loop gain is 0.5 * |EQ| at each frequency
 * (one half from the LR4 branch split, the rest from whatever the EQ is doing),
 * so anywhere the owner asks for bass boost the loop gain exceeds one and the
 * "silence" frame GROWS on every underrun instead of decaying. That is a
 * runaway into an amplifier driving a tweeter whose Fs has not been measured.
 *
 * A guard would work. Owning the buffers removes the defect: here the input
 * buffers and the output buffer are different objects, the zero buffer is
 * written exactly once at allocation and read forever after, and the DSP's
 * destination is never anybody's source. There is no arrangement of underruns
 * that can feed this backend its own output.
 *
 *
 * WHAT THIS BACKEND WILL AND WILL NOT DO
 * ======================================
 * The DSP holds the crossover, the subsonic filter and the two limiters. It is
 * ready only when a calibration profile has been read and built (hk_profile).
 * Until then this backend clocks I2S, drains the receiver and writes DIGITAL
 * ZERO. It does not fall back to passing audio through: an unfiltered full-range
 * signal on the tweeter branch is precisely the damage the profile exists to
 * prevent, and "no calibration" must never be quieter-but-audible. See the
 * Kconfig help text, which says the same thing where somebody selecting this
 * backend will read it.
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

#include <stdlib.h>

/* This project's DSP. The one thing this file has that the vendored one does
 * not, and the reason it is a separate backend rather than a copy. */
#include "hk_dsp.h"

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
 * THE SEAM
 *
 * Every call this backend makes into the DSP is in the three wrappers below
 * and nowhere else, so reconciling with hk_dsp.h is a change to this block
 * rather than a hunt through the playback loop.
 *
 * The contract assumed here:
 *
 *   bool hk_dsp_init(uint32_t sample_rate_hz)
 *        Read the stored calibration and build the chain for this rate.
 *        false means no usable profile -- not an error to retry, a state to
 *        stay silent in. The pack voltage a limiter ceiling has to be
 *        translated to (hk_profile_ceiling_at) is the DSP's business, not this
 *        backend's: this file knows about buffers and DMA, and nothing about
 *        batteries.
 *
 *   bool hk_dsp_ready(void)
 *        Asked once per frame rather than cached, because the DSP may become
 *        unready underneath us -- a profile reload, or a pack voltage that has
 *        left the range its ceilings were measured for.
 *
 *   void hk_dsp_reset(void)
 *        Clear filter and limiter state. Called on flush, where the stream is
 *        discontinuous anyway; without it the tail of the discarded audio rings
 *        out over the first frames of the new.
 *
 *   void hk_dsp_process(const int16_t *in, int16_t *out, size_t frames)
 *        `in` is the interleaved stereo programme; `out` is interleaved
 *        WOOFER (left) / TWEETER (right), per ADR-0002. The mono downmix and
 *        the crossover split both happen inside -- which is why the vendored
 *        apply_channel_mode() has no job in this backend. `in` and `out` are
 *        always distinct buffers here, and `in` is const because this file
 *        guarantees it is never modified.
 * ========================================================================== */

static bool dsp_start(void)
{
    return hk_dsp_init((uint32_t)OUTPUT_RATE);
}

static bool dsp_ready(void)
{
    return hk_dsp_ready();
}

static void dsp_reset(void)
{
    hk_dsp_reset();
}

static void dsp_run(const int16_t *in, int16_t *out, size_t frames)
{
    hk_dsp_process(in, out, frames);
}

/* ========================================================================== */

static i2s_chan_handle_t     tx_handle;
static volatile bool         flush_requested = false;
static volatile bool         playback_running = false;
static TaskHandle_t          playback_task_handle = NULL;
static volatile int          source_rate = 44100;
static volatile bool         resample_reinit_needed = false;

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

/*
 * There is no apply_channel_mode() here, and its absence is a decision rather
 * than an omission.
 *
 * Upstream picks a channel or downmixes because its outputs are a left speaker
 * and a right speaker. Ours are not: ADR-0002 gives the left DAC channel to the
 * woofer and the right to the tweeter, one mono programme split by frequency --
 * "Bu esleme stereo kutu degildir". Choosing a channel downstream of that would
 * not select a channel, it would mute a driver.
 *
 * So the DSP's input mixer makes the selection, and the software downmix stands
 * aside. The channel-mode API itself still answers, from the weak defaults in
 * vendor/audio/audio_output_common.c:33-55 -- set/cycle become no-ops, get
 * reports STEREO and locked reports true, which is what the vendored callers
 * need and is not worth a second copy here.
 */

/* --- The playback task ----------------------------------------------------
 *
 * Structurally the vendored playback_task (vendor/audio/audio_output.c:201-267)
 * with three changes, all of them the point of the file:
 *
 *   1. The zero buffer is const after allocation. It is the DSP's input on an
 *      underrun and the direct I2S write when the DSP is not ready, and NOTHING
 *      writes to it in either role. Compare upstream, where the buffer handed
 *      to I2S is the buffer processing would have written.
 *
 *   2. Output goes to its own buffer. The receiver's PCM, the resampler's
 *      output and the DSP's output are three distinct objects; the DSP's
 *      destination is never anybody's source.
 *
 *   3. The DSP call sits where apply_channel_mode() used to.
 */
static void playback_task(void *arg)
{
    (void)arg;

    /* Input side: what the receiver and resampler fill, and what apply_volume
     * scales in place. Both are refilled completely on every frame that uses
     * them, so in-place work here is not feedback. */
    int16_t *pcm          = malloc((size_t)(FRAME_SAMPLES + 1) * 2 * sizeof(int16_t));
    int16_t *resample_buf = malloc(MAX_RESAMPLE_FRAMES * 2 * sizeof(int16_t));

    /* Output side: the only buffer this task ever hands to I2S when the DSP is
     * running, and the only buffer the DSP writes. Sized for the resampled
     * worst case, because that is the largest frame count that can reach it. */
    int16_t *out = malloc(MAX_RESAMPLE_FRAMES * 2 * sizeof(int16_t));

    /* Zero, once, forever. calloc gives it its only write. */
    const int16_t *zero = calloc((size_t)FRAME_SAMPLES * 2, sizeof(int16_t));

    if (!pcm || !resample_buf || !out || !zero) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(pcm);
        free(resample_buf);
        free(out);
        free((void *)zero);
        playback_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    /* Said once rather than per frame: a log line in the playback loop is a log
     * line at 125 Hz. */
    bool announced_silent = false;

    size_t written;
    while (playback_running) {
        if (resample_reinit_needed) {
            resample_reinit_needed = false;
            audio_resample_init((uint32_t)source_rate, OUTPUT_RATE, 2);
        }
        if (flush_requested) {
            flush_requested = false;
            audio_resample_reset();
            /* Ours: the filters and limiters carry state across the
             * discontinuity too, and the tail of audio that was just discarded
             * would otherwise ring out over the first frames of the new
             * stream. */
            dsp_reset();
            i2s_channel_disable(tx_handle);
            output_cursor_reset();
            i2s_channel_enable(tx_handle);
        }

        /* Read unconditionally, even when the DSP cannot play what is read. The
         * receiver's jitter buffer has to keep draining or the timing engine
         * sees a pipeline that never empties, and the RTSP session that is
         * still perfectly alive backs up behind a silent output stage. */
        size_t samples = audio_receiver_read(pcm, FRAME_SAMPLES + 1);

        if (!dsp_ready()) {
            /* No calibration profile, or one that stopped applying. Digital
             * zero -- not attenuated audio, not audio with the crossover
             * bypassed. The tweeter branch has no protective high-pass and no
             * ceiling in this state, and there is no level at which sending it
             * full-range programme is defensible. */
            if (!announced_silent) {
                announced_silent = true;
                ESP_LOGW(TAG, "no DSP chain: writing digital zero. The receiver "
                              "is running and I2S is clocked, but nothing plays "
                              "until a calibration profile is built (G0/G2).");
            }
            led_audio_feed(zero, FRAME_SAMPLES);
            if (i2s_channel_write(tx_handle, zero,
                                  (size_t)FRAME_SAMPLES * 2 * sizeof(int16_t),
                                  &written, portMAX_DELAY) == ESP_OK) {
                __atomic_add_fetch(&output_submitted_frames,
                                   (uint64_t)(written / (2U * sizeof(int16_t))),
                                   __ATOMIC_RELAXED);
            }
            continue;
        }
        if (announced_silent) {
            announced_silent = false;
            ESP_LOGI(TAG, "DSP chain available; audio is being processed again");
        }

        if (samples > 0) {
            int16_t *play_buf     = pcm;
            size_t   play_samples = samples;
            if (audio_resample_is_active()) {
                play_samples = audio_resample_process(pcm, samples, resample_buf,
                                                      MAX_RESAMPLE_FRAMES);
                play_buf     = resample_buf;
            }
            apply_volume(play_buf, play_samples * 2);

            /* Where apply_channel_mode() used to be, and out of place instead
             * of in it. */
            dsp_run(play_buf, out, play_samples);

            /* Upstream feeds the buffer it is about to write; this feeds the
             * PROGRAMME instead. What we are about to write is two crossover
             * ways, so a level meter fed from it would show woofer-band energy
             * on the left and tweeter-band on the right rather than the
             * loudness of the track. The distinction is currently academic --
             * shim/hk_airplay_shim.c drops these samples, because hk_ui owns
             * the LED -- but the argument is what the next person needs. */
            led_audio_feed(play_buf, play_samples);

            if (i2s_channel_write(tx_handle, out,
                                  play_samples * 2 * sizeof(int16_t), &written,
                                  portMAX_DELAY) == ESP_OK) {
                __atomic_add_fetch(&output_submitted_frames,
                                   (uint64_t)(written / (2U * sizeof(int16_t))),
                                   __ATOMIC_RELAXED);
            }
            taskYIELD();
        } else {
            /* Receiver underflow. Upstream's pacing, kept because the reasoning
             * is still right (vendor/audio/audio_output.c:248-259): block on the
             * DMA write (portMAX_DELAY) so the write itself paces the loop,
             * instead of a short timeout plus vTaskDelay(1) which produced
             * jittery silence.
             *
             * The difference is where the silence comes from. Upstream reuses
             * one buffer as both the thing it processes and the thing it
             * writes; here zeros go IN and the DSP's own output comes OUT. That
             * is not ceremony: it lets the filters and the limiters ring out and
             * recover across the gap instead of freezing mid-decay, so the
             * first frame after the underrun continues the tail rather than
             * stepping off it. And `zero` is untouched by all of it, which is
             * the property upstream's buffer does not have. */
            dsp_run(zero, out, (size_t)FRAME_SAMPLES);
            led_audio_feed(zero, FRAME_SAMPLES);
            if (i2s_channel_write(tx_handle, out,
                                  (size_t)FRAME_SAMPLES * 2 * sizeof(int16_t),
                                  &written, portMAX_DELAY) == ESP_OK) {
                __atomic_add_fetch(&output_submitted_frames,
                                   (uint64_t)(written / (2U * sizeof(int16_t))),
                                   __ATOMIC_RELAXED);
            }
        }
    }

    free(pcm);
    free(resample_buf);
    free(out);
    free((void *)zero);
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
 *   audio_output_set_channel_mode     hk_airplay.c:285                  (weak default exists)
 *
 * The first five have no weak default and MUST be defined here. The next two
 * are defined because this backend genuinely has a completion cursor and a real
 * underrun count, and letting the weak "I cannot answer" default stand would
 * throw away the measurement the timing engine prefers. The channel-mode calls
 * are deliberately left to the weak defaults; see the note above the playback
 * task.
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
     * restore (this backend has no channel mode) and plus the DSP bring-up. */
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

    /* Ours. A failure is not an init failure: the receiver, the clock and the
     * network side are all perfectly able to run, and refusing to start here
     * would take AirPlay discovery down over a missing calibration file. The
     * playback task writes zero instead, and says so. */
    if (!dsp_start()) {
        ESP_LOGW(TAG, "DSP chain not built -- no calibration profile. The output "
                      "will be digital zero until one exists (G0/G2).");
    } else {
        ESP_LOGI(TAG, "DSP chain built at %u Hz: left = woofer, right = tweeter "
                      "(ADR-0002). This is not a stereo pair.",
                 (unsigned int)OUTPUT_RATE);
    }

    return ESP_OK;
}

void audio_output_start(void)
{
    /* Copied from vendor/audio/audio_output.c:356-368, including the task
     * priority (AUDIO_PLAYBACK_TASK_PRIORITY, which audio_output.h explains
     * must outrank every source task) and the core pinning. */
    if (playback_task_handle != NULL) {
        return; /* already running */
    }
    playback_running = true;
    /* The DMA has been free-running since the last session, so the cursor
     * carries an arbitrary submitted/sent skew. Start from a clean slate. */
    output_cursor_reset();
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
     * The DSP adds no latency of its own to model: the filters are biquads and
     * the limiter has zero attack and no lookahead, both of which hk_limiter.h
     * chose precisely so this number would stay true. */
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
