#include "hk_audio_hw.h"

#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_audio.h"
#include "hk_pins.h"

static const char *TAG = "hk_audio";

/*
 * Both lines are ACTIVE LOW. Low is muted, high is released.
 *
 * Written as named constants rather than as bare 0 and 1 because this is the
 * one polarity in the firmware that must not be got wrong by a reader in a
 * hurry: inverting it does not produce a device that stays quiet when it should
 * play, it produces a device that plays when it should be quiet, into drivers
 * whose impedance is the open G0 blocker.
 */
#define HK_MUTE_ASSERTED  0u  /**< Muted. The level the pad already has at reset. */
#define HK_MUTE_RELEASED  1u  /**< Sound allowed through. */

#define HK_AUDIO_HW_MUTE_MASK \
    ((1ULL << HK_PIN_AMP_MUTE) | (1ULL << HK_PIN_DAC_XSMT))

/*
 * hk_pins.h states this rule in prose; here it is as a compile error.
 *
 * An active-low mute is only safe on a pad whose reset level is LOW. The
 * ESP32-S3 drives GPIO18, 19 and 20 HIGH during power-up, and GPIO0, 39, 43 and
 * 44 come up with weak internal pull-ups. A mute line on any of those is
 * released before software exists — through the ROM, the second-stage
 * bootloader and app init, hundreds of milliseconds during which an amplifier
 * would be free to reproduce whatever is on its input.
 *
 * The prose lives next to the pin table and this assert lives next to the code
 * that drives the pins, because the two files are edited by different people
 * for different reasons and a rule that only exists in a comment is a rule that
 * survives exactly as long as the comment is read.
 */
#define HK_PIN_RESET_NOT_LOW_MASK ( \
      (1ULL << 18) | (1ULL << 19) | (1ULL << 20) | \
      (1ULL << 0)  | (1ULL << 39) | (1ULL << 43) | (1ULL << 44))

_Static_assert((HK_AUDIO_HW_MUTE_MASK & HK_PIN_RESET_NOT_LOW_MASK) == 0,
               "hk_audio_hw: a mute line sits on a pad the silicon drives high or "
               "pulls up at reset, so it would be RELEASED before this firmware runs");
_Static_assert(HK_PIN_AMP_MUTE != HK_PIN_DAC_XSMT,
               "hk_audio_hw: the amplifier and DAC mute lines share a GPIO, so the "
               "sequence cannot move one without the other");
_Static_assert((HK_AUDIO_HW_MUTE_MASK & HK_PIN_FORBIDDEN_MASK) == 0,
               "hk_audio_hw: a mute line lands on a reserved pin");

/**
 * Settle times.
 *
 * THESE ARE PROVISIONAL. They are reasoning, not measurement, and they belong
 * to G1 (the amplifier on a dummy load) and G3 (the loaded rails), neither of
 * which has been run. hk_audio.h leaves hk_audio_timing_t without defaults for
 * exactly this reason: a number invented here is indistinguishable from a
 * measured one once it is in the source. So it is said here, once, plainly, and
 * the values are logged at start so the boot record shows what was used rather
 * than what someone assumed.
 *
 * The reasoning behind each, so that the G1/G3 operator knows what to check:
 *
 *   clock_settle_ms  The PCM5102A runs in 3-wire mode with SCK grounded and
 *                    recovers its clock from BCK with an internal PLL
 *                    (ADR-0002, and the absence of an MCLK pin in hk_pins.h).
 *                    How long that PLL takes to lock on this board is not
 *                    known here. 200 ms is generous on purpose: being early
 *                    puts a step on the DAC output, and that step is then
 *                    multiplied by the amplifier's gain; being late costs a
 *                    fifth of a second of silence at the start of a track,
 *                    which nobody will notice.
 *
 *   dac_settle_ms    Releasing XSMT ramps the DAC's output rather than
 *                    switching it, and the amplifier must not be live during
 *                    that ramp. Same trade, same direction, same value.
 *
 *   mute_settle_ms   The unwind. The amplifier is already shut down by the
 *                    time this is being waited out, so nothing is being
 *                    amplified and a generous value costs nothing audible.
 *                    50 ms is longer than any plausible SD-to-output-off time
 *                    for a TPA3110-class part — but note that this project has
 *                    not confirmed the amplifier board even exposes an
 *                    accessible SD pad: hk_pins.h calls HK_PIN_AMP_MUTE a
 *                    RESERVATION. If it is not connected, this line moves and
 *                    nothing happens, and the operator will only find that out
 *                    by measuring.
 */
static const hk_audio_timing_t HK_AUDIO_HW_TIMING = {
    .clock_settle_ms = 200,
    .dac_settle_ms   = 200,
    .mute_settle_ms  = 50,
};

/**
 * How often the sequence is advanced.
 *
 * Finer than the shortest settle time by a wide margin, so the settle times are
 * the thing that decides the sequence rather than the tick being it. Coarse
 * enough that a task woken 100 times a second costs nothing measurable next to
 * a Wi-Fi radio.
 */
#define HK_AUDIO_HW_TICK_MS 10

/*
 * Small: this task reads two bools, runs a switch and writes two registers. It
 * calls nothing that allocates and nothing that formats, except on a state
 * change, which is where the ESP_LOG headroom is spent.
 */
#define HK_AUDIO_HW_TASK_STACK 2560

/*
 * Above the LED, below everything real-time.
 *
 * hk_ui runs at 2 with the note that audio and networking must both pre-empt
 * it. Both must pre-empt this one too — an amplifier's mute line does not need
 * to be serviced before an I2S buffer, and a task that could delay the audio
 * path would be a strange thing to add in the name of audio safety. It sits one
 * step above the LED renderer for the plain reason that this one drives an
 * amplifier and that one drives a light.
 */
#define HK_AUDIO_HW_TASK_PRIO 3

static hk_audio_t   s_chain;
static TaskHandle_t s_task;

/*
 * Read together under one lock.
 *
 * They are two independent bools written from two different tasks — the
 * application's supervisory loop sets permission, the receiver's RTSP task sets
 * the stream — but the sequencer consumes them as a PAIR: `permitted &&
 * stream_live` is one question. Taking them separately would let a tick see
 * permission from before a revocation and a stream from after it.
 */
static bool         s_permitted;
static bool         s_stream_live;
static portMUX_TYPE s_input_lock = portMUX_INITIALIZER_UNLOCKED;

/** Milliseconds since boot. */
static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * Put the two pins where @p out says they should be.
 *
 * The amplifier is written FIRST when it is going down and LAST when it is
 * coming up, unconditionally, rather than only on the transitions where that
 * happens to matter. Today the sequencer never moves both lines in one step, so
 * the ordering is invisible; the day a state is added that does, this function
 * is already right. Getting it wrong the other way round sends the DAC's own
 * transition through a live amplifier, which is the exact thump the sequence in
 * hk_audio.c exists to prevent.
 *
 * i2s_running is deliberately not acted on: the AirPlay receiver owns the I2S
 * peripheral. See the header.
 */
static void apply(hk_audio_outputs_t out)
{
    if (!out.amp_enabled) {
        (void)gpio_set_level((gpio_num_t)HK_PIN_AMP_MUTE, HK_MUTE_ASSERTED);
    }
    (void)gpio_set_level((gpio_num_t)HK_PIN_DAC_XSMT,
                         out.dac_unmuted ? HK_MUTE_RELEASED : HK_MUTE_ASSERTED);
    if (out.amp_enabled) {
        (void)gpio_set_level((gpio_num_t)HK_PIN_AMP_MUTE, HK_MUTE_RELEASED);
    }
}

/**
 * Drive both lines to the safe level, then enable the output drivers.
 *
 * The order is the point. gpio_set_level() writes the output data register
 * whether or not the pad is an output yet, so by the time gpio_config() enables
 * the driver the register already holds the asserted level and the pad never
 * drives the released one — not for a single instruction. Configuring first and
 * setting after would open a window of undefined output level on an
 * amplifier's shutdown pin, which is a small window and the wrong pin.
 *
 * The internal pull-downs are enabled as well. They are not the mechanism —
 * hk_pins.h is explicit that a 10 k external pull-down is, and that it dominates
 * the part's own weak pull four to one — but they cost nothing and they keep the
 * line defined on a board where that resistor has not been fitted yet.
 */
static esp_err_t configure_mute_pins(void)
{
    esp_err_t err = gpio_set_level((gpio_num_t)HK_PIN_AMP_MUTE, HK_MUTE_ASSERTED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "amplifier mute gpio%d: %s", HK_PIN_AMP_MUTE, esp_err_to_name(err));
        return err;
    }
    err = gpio_set_level((gpio_num_t)HK_PIN_DAC_XSMT, HK_MUTE_ASSERTED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dac xsmt gpio%d: %s", HK_PIN_DAC_XSMT, esp_err_to_name(err));
        return err;
    }

    const gpio_config_t muted = {
        .pin_bit_mask = HK_AUDIO_HW_MUTE_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&muted);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mute lines gpio%d/%d: %s",
                 HK_PIN_AMP_MUTE, HK_PIN_DAC_XSMT, esp_err_to_name(err));
    }
    return err;
}

static void audio_hw_task(void *arg)
{
    (void)arg;

    TickType_t       last_wake = xTaskGetTickCount();
    hk_audio_state_t reported = s_chain.state;

    while (true) {
        hk_audio_inputs_t inputs;
        portENTER_CRITICAL(&s_input_lock);
        inputs.permitted = s_permitted;
        inputs.stream_live = s_stream_live;
        portEXIT_CRITICAL(&s_input_lock);
        inputs.now_ms = now_ms();

        hk_audio_step(&s_chain, &inputs, &HK_AUDIO_HW_TIMING);
        const hk_audio_outputs_t out = hk_audio_outputs(s_chain.state);
        apply(out);

        /* Logged on change only. A line per tick would bury everything else in
         * the console, and the transitions are the whole story: which way the
         * chain moved, and whether the amplifier really was the last thing up
         * and the first thing down. */
        if (s_chain.state != reported) {
            ESP_LOGI(TAG, "%s -> %s (dac=%d amp=%d, permitted=%d stream=%d)",
                     hk_audio_state_name(reported),
                     hk_audio_state_name(s_chain.state),
                     out.dac_unmuted, out.amp_enabled,
                     inputs.permitted, inputs.stream_live);
            reported = s_chain.state;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HK_AUDIO_HW_TICK_MS));
    }
}

void hk_audio_hw_set_permitted(bool permitted)
{
    portENTER_CRITICAL(&s_input_lock);
    s_permitted = permitted;
    portEXIT_CRITICAL(&s_input_lock);
}

void hk_audio_hw_set_stream_live(bool live)
{
    portENTER_CRITICAL(&s_input_lock);
    s_stream_live = live;
    portEXIT_CRITICAL(&s_input_lock);
}

esp_err_t hk_audio_hw_start(void)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Pins before state before task: at no point does a running task see a
     * chain it has not been given, and at no point is a pad an output at a
     * level nobody chose. */
    const esp_err_t err = configure_mute_pins();
    if (err != ESP_OK) {
        return err;
    }
    hk_audio_init(&s_chain, now_ms());
    apply(hk_audio_outputs(s_chain.state));

    const BaseType_t created = xTaskCreate(audio_hw_task, "hk_audio",
                                           HK_AUDIO_HW_TASK_STACK, NULL,
                                           HK_AUDIO_HW_TASK_PRIO, &s_task);
    if (created != pdPASS) {
        /* The pins keep the level they were just given, and nothing will ever
         * release them, which is the right way for this to fail. */
        s_task = NULL;
        ESP_LOGE(TAG, "could not create the audio task; the mute lines stay asserted");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "mute lines driven: amp gpio%d, dac xsmt gpio%d, both active low "
                  "and both ASSERTED (%s)",
             HK_PIN_AMP_MUTE, HK_PIN_DAC_XSMT, hk_audio_state_name(s_chain.state));
    ESP_LOGW(TAG, "settle times %" PRIu32 "/%" PRIu32 "/%" PRIu32 " ms "
                  "(clock/dac/mute) are PROVISIONAL: reasoned, not measured. "
                  "They belong to G1 and G3.",
             HK_AUDIO_HW_TIMING.clock_settle_ms,
             HK_AUDIO_HW_TIMING.dac_settle_ms,
             HK_AUDIO_HW_TIMING.mute_settle_ms);
    return ESP_OK;
}
