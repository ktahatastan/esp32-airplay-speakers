#include "hk_tone.h"

#include <math.h>
#include <stdint.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_pins.h"

static const char *TAG = "hk_tone";

/* --- The signal ---------------------------------------------------------- */

/**
 * 1 kHz. Near the middle of the woofer's range and nowhere near the crossover
 * region this project has not designed yet, so it is reproduced by whichever
 * driver happens to be connected and asks nothing of the one that is not.
 * It is also the frequency a person can identify as "a tone" rather than as
 * "a noise" from across a room, which is the entire measurement being taken.
 */
#define HK_TONE_HZ      1000

/** AirPlay's own rate, so the I2S clock tree is configured exactly as the
 *  receiver configures it and a difference in result is not a difference in
 *  setup. See CONFIG_OUTPUT_SAMPLE_RATE_HZ in components/hk_airplay/Kconfig. */
#define HK_TONE_RATE_HZ 44100

/**
 * AMPLITUDE. THIS IS THE SAFETY-CRITICAL NUMBER IN THIS FILE.
 *
 * -30 dBFS, which is 10^(-30/20) = 0.0316 of full scale: about 1/32nd of the
 * peak the format can carry, or 1036 of the 32767 counts a 16-bit sample has.
 *
 * It is low because nothing downstream of it is known:
 *
 *   - The individual Nova woofer and tweeter impedances have never been
 *     measured. That is an open `Kritik` blocker (AGENTS.md, and
 *     docs/02-hardware/driver-measurements.md), and it is what decides the
 *     safe amplifier level. Without it, "how much power does this become at
 *     the driver" has no answer at all -- not a conservative one, not any.
 *   - There is no crossover and no protective high-pass. A tweeter connected
 *     to this output sees the full band.
 *   - There is no limiter. Firmware stage F3 owns all three of those and F3
 *     waits on the G0 measurement.
 *
 * So between this constant and a voice coil there is a power amplifier with a
 * fixed gain and nothing else. Whatever number is written here is multiplied
 * and delivered. -30 dBFS is quiet enough to be survivable into an unknown
 * load and loud enough to be unmistakably audible in a quiet room, which is
 * the whole job: this is a diagnostic, not a listening test.
 *
 * Raising it is a hardware-safety change and belongs to G0/G1, not to whoever
 * is at the bench. The assert below is the floor under that: it refuses to
 * compile above -20 dBFS (3277 counts) so the number cannot be nudged upward
 * in a hurry without someone deliberately editing the limit as well.
 */
#define HK_TONE_DBFS     (-30)
#define HK_TONE_PEAK_LSB 1036   /* round(10^(-30/20) * 32767) */

_Static_assert(HK_TONE_PEAK_LSB > 0,
               "hk_tone: an inaudible tone measures nothing");
_Static_assert(HK_TONE_PEAK_LSB <= 3277,
               "hk_tone: amplitude above -20 dBFS into drivers whose impedance is "
               "the open G0 blocker, with no crossover, no high-pass and no limiter "
               "in front of the amplifier. Raise this only with G0/G1 evidence.");

/**
 * One period-aligned block, generated once and repeated forever.
 *
 * 1 kHz at 44.1 kHz is 44.1 samples per cycle, which is not a whole number --
 * so a single-cycle table would step in phase every time it wrapped, and the
 * result would be a 1 kHz tone with a 44.1 Hz buzz welded onto it. Ten cycles
 * is the smallest block that lands exactly on a sample boundary: 441 frames.
 * Repeating THAT is phase-continuous, and it is the difference between an
 * operator hearing a clean tone and hearing something they will report as
 * distortion in the DAC.
 *
 * The static assert is the arithmetic, so changing either frequency or rate to
 * a pair that does not divide fails at compile time instead of on the bench.
 */
#define HK_TONE_CYCLES 10
#define HK_TONE_FRAMES 441

_Static_assert(HK_TONE_FRAMES * HK_TONE_HZ == HK_TONE_CYCLES * HK_TONE_RATE_HZ,
               "hk_tone: the block is not a whole number of cycles, so repeating it "
               "would put a discontinuity into the tone at every wrap");

/** Stereo, interleaved, 16-bit: the format the DAC is configured for. */
static int16_t s_frames[HK_TONE_FRAMES * 2];

/* --- The peripheral ------------------------------------------------------ */

/*
 * Deliberately identical to the vendored receiver's own configuration
 * (vendor/audio/audio_output.c): I2S_NUM_0, master, 8 descriptors of 256
 * frames, auto_clear, Philips 16-bit stereo, no MCLK. If the tone behaves and
 * AirPlay does not, that difference must not be attributable to how the bus
 * was set up.
 */
#define HK_TONE_DMA_DESC_NUM  8
#define HK_TONE_DMA_FRAME_NUM 256

/** The vendored playback task's priority and core, for the same reason. */
#define HK_TONE_TASK_PRIO  9
#define HK_TONE_TASK_CORE  1
#define HK_TONE_TASK_STACK 3072

static i2s_chan_handle_t s_tx;
static TaskHandle_t      s_task;

static void fill_block(void)
{
    /* sinf() runs here, 441 times, once in the life of the process. The write
     * loop below does nothing but hand the same bytes to the DMA: a transcend-
     * ental per sample on the audio path would be a strange thing to add to a
     * firmware whose open question is whether samples arrive on time. */
    for (int i = 0; i < HK_TONE_FRAMES; i++) {
        const float phase = 2.0f * (float)M_PI * (float)HK_TONE_CYCLES
                          * (float)i / (float)HK_TONE_FRAMES;
        const int16_t sample = (int16_t)lrintf((float)HK_TONE_PEAK_LSB * sinf(phase));
        /* The same sample in both slots. The XH-A232 is two channels of one
         * amplifier and the operator may have only one of them wired; a tone
         * that came out of one channel would be indistinguishable from a
         * broken channel. */
        s_frames[2 * i]     = sample;
        s_frames[2 * i + 1] = sample;
    }
}

static void tone_task(void *arg)
{
    (void)arg;

    bool complained = false;
    while (true) {
        size_t written = 0;
        /* Blocks until the DMA ring has room, which is what paces this loop:
         * no delay, no timer, the bus itself is the clock. */
        const esp_err_t err = i2s_channel_write(s_tx, s_frames, sizeof(s_frames),
                                                &written, portMAX_DELAY);
        if (err != ESP_OK) {
            if (!complained) {
                complained = true;
                ESP_LOGE(TAG, "i2s write failed: %s. The tone has stopped; the pins "
                              "are still configured, so a scope will still see clocks.",
                         esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            complained = false;
        }
    }
}

esp_err_t hk_tone_start(void)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = HK_TONE_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = HK_TONE_DMA_FRAME_NUM;
    chan_cfg.auto_clear    = true;

    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err));
        return err;
    }

    /* The pins come from hk_pins.h and are not repeated as literals here. The
     * receiver reads the same three numbers out of its own Kconfig and
     * hk_airplay.c static-asserts that the two agree, so all three consumers
     * of this assignment trace back to one table. */
    const i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(HK_TONE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            /* No MCLK. The PCM5102A runs 3-wire with SCK strapped to ground and
             * recovers its clock from BCK (ADR-0002). If that strap is missing
             * on the board in hand, this tone will be noise -- which is one of
             * the things the operator is being asked to listen for. */
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)HK_PIN_I2S_BCLK,
            .ws   = (gpio_num_t)HK_PIN_I2S_LRCLK,
            .dout = (gpio_num_t)HK_PIN_I2S_DATA,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode: %s", esp_err_to_name(err));
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
        return err;
    }

    fill_block();

    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable: %s", esp_err_to_name(err));
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
        return err;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(tone_task, "hk_tone",
                                                       HK_TONE_TASK_STACK, NULL,
                                                       HK_TONE_TASK_PRIO, &s_task,
                                                       HK_TONE_TASK_CORE);
    if (created != pdPASS) {
        s_task = NULL;
        (void)i2s_channel_disable(s_tx);
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
        ESP_LOGE(TAG, "could not create the tone task");
        return ESP_ERR_NO_MEM;
    }

    /* Everything the operator needs to act on the result, in one place.
     *
     * Printed after the tone is actually running, so a line that appears is a
     * line that was true. Four lines and not one, because the amplitude is a
     * warning and the interpretation is a table, and a single paragraph in a
     * serial console is a paragraph nobody reads at 2am. */
    ESP_LOGI(TAG, "TEST TONE RUNNING: %d Hz sine, %d Hz, 16-bit stereo, "
                  "bck gpio%d, ws gpio%d, data gpio%d",
             HK_TONE_HZ, HK_TONE_RATE_HZ,
             HK_PIN_I2S_BCLK, HK_PIN_I2S_LRCLK, HK_PIN_I2S_DATA);
    ESP_LOGW(TAG, "amplitude %d dBFS (%d of 32767 counts, about 1/32 of full scale). "
                  "Low on purpose: driver impedance is unmeasured (G0) and there is no "
                  "crossover, no high-pass and no limiter (F3), so the amplifier gets "
                  "this with nothing in front of it.",
             HK_TONE_DBFS, HK_TONE_PEAK_LSB);
    ESP_LOGI(TAG, "the AirPlay receiver is NOT running in this build; these samples are "
                  "generated locally, so nothing that follows involves the network");
    ESP_LOGI(TAG, "WHAT YOU HEAR: clean steady tone -> I2S, DAC, amplifier and speaker "
                  "are all good and the fault is upstream in the AirPlay path. "
                  "Noise/hiss but no tone -> the bus clocks and the analogue chain is "
                  "wrong: DAC power, the SCK-to-GND strap, the data line, the amplifier "
                  "input or its ground. Nothing at all -> check the hk_audio lines "
                  "above: if the sequence never left SILENT the mute lines are still "
                  "asserted and nothing was ever going to be heard.");
    return ESP_OK;
}
