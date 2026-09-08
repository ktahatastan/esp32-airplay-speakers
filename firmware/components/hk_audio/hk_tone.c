#include "hk_tone.h"

#include <math.h>
#include <stdint.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_pins.h"

static const char *TAG = "hk_tone";

/** AirPlay's own rate, so the I2S clock tree is configured exactly as the
 *  receiver configures it and a difference in result is not a difference in
 *  setup. See CONFIG_OUTPUT_SAMPLE_RATE_HZ in components/hk_airplay/Kconfig. */
#define HK_TONE_RATE_HZ 44100

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

#if CONFIG_HK_BENCH_SWEEP

/* ========================================================================= *
 *  MODE: STEPPED SWEEP -- driver impedance and resonance (G0)
 * ========================================================================= */

/* Told once, when the signal ends. Declared only in this mode, because the
 * fixed tone never ends: a callback held by a signal that cannot stop is a
 * callback somebody eventually expects to fire. */
static hk_tone_done_fn s_done;
static void           *s_done_context;

static void signal_ended(void)
{
    if (s_done != NULL) {
        s_done(s_done_context);
    }
}

/**
 * WHAT IS BEING MEASURED, AND WHY IT CANNOT BE GUESSED.
 *
 * The Nova drivers carry manufacturer internal codes and nothing else, and the
 * woofer has an extra magnet glued to the back of its motor, so no published
 * parameter for "that model" applies to THIS driver even if one were found.
 * The DC resistances are known (woofer 4.0 ohm, tweeter 3.5 ohm, operator,
 * 2026-09-08) and a single resistance is not an impedance curve.
 *
 * The number this exists to produce is the tweeter's Fs, because Fs sets the
 * MINIMUM SAFE high-pass corner -- the rule of thumb is at least twice Fs, and
 * a high-pass placed below a tweeter's resonance is a high-pass that lets the
 * amplifier drive the one frequency at which the dome moves furthest for the
 * least voltage. Until Fs is measured, the crossover corner in
 * docs/04-acoustics/measurement-and-dsp-plan.md is a conservative guess, and
 * this project does not run guesses it could have measured.
 *
 * THE CIRCUIT, because the amplitude below only makes sense against it:
 *
 *     DAC line out ---[ Rs = 470 ohm ]---+--- driver +
 *                                        |
 *                                     (driver)
 *                                        |
 *     DAC ground ------------------------+--- driver -
 *
 *                          meter on AC volts, across the driver
 *
 * With Rs far larger than the driver's impedance the current is very nearly
 * constant, so the voltage the meter reads is proportional to |Z(f)|, and the
 * frequency at which it PEAKS is Fs. The full arithmetic, the expected values
 * and the table to fill in are in docs/02-hardware/driver-measurements.md.
 *
 * THE AMPLIFIER IS NOT IN THIS CIRCUIT. That is the whole safety argument, and
 * it is also the thing firmware cannot check. See the header.
 */

/**
 * AMPLITUDE. THIS IS THE SAFETY-CRITICAL NUMBER IN THIS MODE.
 *
 * -6 dBFS, which is 10^(-6/20) = 0.501 of full scale: 16422 of the 32767 counts
 * a 16-bit sample has. That is 24 dB ABOVE the fixed tone below, and the
 * increase has to be justified rather than assumed, so here is the whole of it.
 *
 * 1. IT IS A DIFFERENT CIRCUIT. The fixed tone's -30 dBFS is chosen for what
 *    sits downstream of it: a class-D amplifier of unknown and possibly 36 dB
 *    gain (docs/06-testing/bench-measurement-order.md, C3), feeding an
 *    unmeasured driver with no crossover, no protective high-pass and no
 *    limiter. Whatever is written there is multiplied by that gain and
 *    delivered. Here the amplifier is absent and 470 ohms is in its place. The
 *    PCM5102A gives about 2.1 Vrms at full scale, so -6 dBFS is ~1.05 Vrms
 *    behind 470 ohms: about 2.2 mA rms through the driver, whatever the driver
 *    does. That is ~0.02 mW in a 4 ohm coil off resonance and under half a
 *    milliwatt even at a 200 ohm peak, which is far taller than anything these
 *    drivers can produce. Milliwatts is not a figure of speech here.
 *
 * 2. IT IS THE LEVEL THAT FITS A CHEAP METER'S RANGE. The point of the whole
 *    exercise is a number an operator can read off a 3.5-digit multimeter. At
 *    -6 dBFS the voltage across the driver runs from about 8 mV where Z is
 *    closest to Re up to about 150 mV at a tall resonance peak -- the entire
 *    curve inside ONE 200 mV AC range, so the meter never autoranges mid-step
 *    and no reading is lost to it. At 0 dBFS the peak would exceed 200 mV and
 *    force a range change exactly where the interesting data is; at -20 dBFS,
 *    the fixed tone's ceiling, the floor reading is under 2 mV and a cheap
 *    meter cannot resolve it at all.
 *
 * 3. IT KEEPS 6 dB OF DIGITAL HEADROOM, so rounding, any DC offset and the
 *    DAC's reconstruction-filter ripple cannot clip a sine that is meant to be
 *    a sine.
 *
 * The two asserts below are the floor and the ceiling of that argument, in the
 * same shape the fixed tone's are: raising the level requires deliberately
 * editing the limit as well, and lowering it past the point where the meter
 * can read anything fails too, because a measurement nobody can read measures
 * nothing.
 */
#define HK_SWEEP_DBFS     (-6)
#define HK_SWEEP_PEAK_LSB 16422   /* round(10^(-6/20) * 32767) */

_Static_assert(HK_SWEEP_PEAK_LSB <= 16422,
               "hk_tone: sweep amplitude above -6 dBFS. This level is chosen for a "
               "line output behind a 470 ohm series resistor, where it is under a "
               "milliwatt; it is NOT a level for the amplifier path, and raising it "
               "gains nothing a multimeter can use. Raise it only with G0 evidence.");
_Static_assert(HK_SWEEP_PEAK_LSB >= 8231,
               "hk_tone: below -12 dBFS the voltage across a 4 ohm driver behind 470 "
               "ohms is under 5 mV, which a 3.5-digit meter cannot resolve. A sweep "
               "nobody can read measures nothing.");

/**
 * The series resistor this mode is documented and logged against.
 *
 * 470 ohm, 1/4 W, and every part of that is a consequence:
 *
 *   - It is more than 100x the drivers' DC resistance, so treating the current
 *     as constant is good to about 1% away from resonance and about 6% at a
 *     30 ohm peak -- and the doc gives the exact correction for the peak, which
 *     is where the error actually lives.
 *   - It leaves the DAC driving ~474 ohms, a light load for a line output. A
 *     smaller resistor would give bigger readings and load the PCM5102A's
 *     output stage towards headphone territory, where its distortion is not
 *     specified and the measurement stops being about the driver.
 *   - It puts the whole curve inside one 200 mV meter range at -6 dBFS (above).
 *   - It dissipates 2.1^2/470 = 9.4 mW, so a quarter-watt part is not close.
 *   - It is an E12 value, which matters more than it should: the operator has
 *     to actually own one.
 *
 * It appears in the log because the arithmetic that turns millivolts into ohms
 * needs it, and a bench where the resistor and the firmware disagree about its
 * value produces a plausible, wrong impedance curve.
 */
#define HK_SWEEP_SERIES_OHM 470

/**
 * THE FREQUENCY GRID, and why it is expressed as frames-per-cycle.
 *
 * Each step is generated the way the fixed tone is: one block holding a whole
 * number of cycles, written over and over. That block MUST be a whole number of
 * cycles or every wrap puts a discontinuity into the tone, which is the reason
 * the fixed tone below uses 441 frames for 1 kHz rather than 44.
 *
 * The tidy way to get that for an arbitrary frequency is to stop choosing
 * frequencies and start choosing k, the number of frames in one cycle:
 *
 *     f = 44100 / k     exactly, for any integer k
 *
 * One cycle is then exactly k frames, and any number of repeats of it is still
 * a whole number of cycles. Nothing has to be rounded, and the frequency that
 * gets logged is the frequency that gets generated.
 *
 * The resolution this gives is (k+1)/k, which is finest at low frequencies and
 * coarsest at high ones -- 0.1% at 40 Hz, 2% at 1 kHz, 9% at 4 kHz. That is the
 * right way round for this measurement by luck rather than design, but it is
 * worth stating: the woofer's resonance is low and needs resolution, and the
 * tweeter's Fs only has to be good enough to put a high-pass an octave above
 * it, where a few percent is nothing.
 */
#define HK_SWEEP_K_MIN 6     /* 7350 Hz -- above any dome tweeter's Fs */
#define HK_SWEEP_K_MAX 2205  /* 20 Hz  -- below any 60 mm cone's Fs */

/**
 * THE COARSE PASS: 22 steps at 1/3 octave from 40 Hz to 4.9 kHz.
 *
 * THE RANGE. It has to contain the resonance of both drivers, and they are
 * nowhere near each other.
 *
 *   - The 60 mm cone woofer should resonate somewhere around 100-250 Hz in free
 *     air. Its double magnet stack raises Bl, which mostly lowers Qes and so
 *     RAISES the height of the peak; it moves Fs much less, because Fs is set
 *     by the moving mass and the suspension and the magnets are on neither.
 *     Expect a tall peak at roughly where a driver that size would put one.
 *   - The 25 mm dome tweeter should resonate around 1200-2000 Hz, which is the
 *     range the C_SAFE section of docs/02-hardware/driver-measurements.md
 *     already reasons from. This sweep is the measurement that section says it
 *     is waiting for.
 *
 * 40 Hz is the bottom because it is comfortably below any plausible woofer Fs
 * AND because it is where a cheap multimeter's AC volts specification starts --
 * most are rated from 40 or 45 Hz, and below that the reading is not wrong so
 * much as undefined. 4.9 kHz at the top is well over an octave above the
 * highest Fs a 25 mm dome could plausibly have, which is what it takes to see
 * that a peak is a peak and not the edge of the range.
 *
 * LOG SPACING, because resonance is a ratio phenomenon: a 20 Hz error matters
 * enormously at 100 Hz and not at all at 4 kHz, so a linear grid would waste
 * most of its points where they cannot help. 1/3 octave is 26% per step, which
 * puts two or three points on a resonance peak of ordinary sharpness -- enough
 * to see the peak and to say which step it is on, not enough to say Fs to
 * better than about 13%. That is what the fine pass is for. Twenty-two rows is
 * also about as much hand transcription as a person will actually finish; the
 * 1/6-octave version is 43 rows and would be abandoned halfway.
 *
 * The values are ISO 1/3-octave centres rounded to the nearest available k, so
 * the operator's table reads 40, 50, 63, 80 ... and the log prints the exact
 * frequency generated, which drifts from the nominal by up to 2% at the top.
 */
/* Built only in a build that runs it, for the same reason the fine grid is:
 * one image runs one pass, and a table compiled into the other one is a table
 * the linker has to be asked to forgive. */
#if CONFIG_HK_BENCH_SWEEP_FINE_HZ == 0
static const uint16_t HK_SWEEP_COARSE_K[] = {
    /*  nominal:  40    50    63    80   100   125   160   200   250   315  */
    /*  actual:   40.0  50.0  63.0  80.0 100.0 124.9 159.8 199.5 250.6 315.0 */
              1102,  882,  700,  551,  441,  353,  276,  221,  176,  140,
    /*  nominal: 400   500   630   800  1000  1250  1600  2000  2500  3150  */
    /*  actual:  400.9 501.1 630.0 801.8 1002  1260  1575  2004  2450  3150 */
               110,   88,   70,   55,   44,   35,   28,   22,   18,   14,
    /*  nominal: 4000  5000                                                 */
    /*  actual:  4009  4900                                                 */
                11,    9,
};
#define HK_SWEEP_COARSE_STEPS \
    (sizeof(HK_SWEEP_COARSE_K) / sizeof(HK_SWEEP_COARSE_K[0]))
#endif /* CONFIG_HK_BENCH_SWEEP_FINE_HZ == 0 */

/**
 * THE FINE PASS: 17 steps at 1/12 octave, spanning +/- 2/3 octave.
 *
 * Run second, centred on the peak the coarse pass found, by setting
 * CONFIG_HK_BENCH_SWEEP_FINE_HZ and reflashing. 1/12 octave is 5.9% per step,
 * which locates Fs to about 3%; +/- 2/3 octave is twice the coarse pass's own
 * step size, so the true peak is inside this window even if the coarse pass
 * named the wrong neighbour.
 *
 * Two passes rather than one long fine sweep because a 1/12-octave sweep of the
 * whole 40 Hz - 5 kHz range is 122 steps and sixteen minutes of a person
 * copying numbers off a meter, and the middle eighty of those steps are known
 * in advance to be flat. Coarse-then-fine costs six minutes and produces a
 * better number.
 *
 * The ratios are 2^(n/12) for n = -8..+8 in 1/1024ths, so the frequency grid is
 * built with integer arithmetic only. Not a performance concern -- it runs
 * seventeen times at startup -- but a determinism one: the frequencies in the
 * log are exactly the frequencies in the doc, on every build, with no float
 * rounding standing between them.
 */
#define HK_SWEEP_FINE_STEPS 17

/* The grid itself is built only in a build that asked for a fine pass. The
 * count above is not, because the coarse build's closing log line offers the
 * fine pass by name and has to say how many steps it would be. */
#if CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0
static const uint16_t HK_SWEEP_FINE_RATIO_1024[HK_SWEEP_FINE_STEPS] = {
     645,  683,  724,  767,  813,  861,  912,  967, 1024,
    1085, 1149, 1218, 1290, 1367, 1448, 1534, 1625,
};
#endif

/**
 * DWELL: 8 seconds a step, and it is chosen for the instrument, not the driver.
 *
 * A 3.5-digit multimeter on AC volts is an averaging instrument: it updates two
 * or three times a second and its reading takes a second or two to stop moving
 * after the input changes. If it autoranges -- and it will, if the operator
 * left it on auto and the sweep walks up a resonance peak -- add another second
 * or two. Call it three seconds before the display is trustworthy.
 *
 * Then a person has to look at it, read three or four digits, find the right
 * row, and write them down. Five seconds is not generous for that; it is
 * roughly what it takes when you are also watching a serial console to see
 * which row you are on.
 *
 * Eight seconds is those two added together. The whole coarse pass is then
 * about three and a half minutes, which is short enough that nobody is tempted
 * to walk away from it, and the run needs no input at all -- a sweep that asked
 * for a button press between steps would be a sweep that gets abandoned on step
 * nine, and the half-finished table would be worse than none because somebody
 * would try to draw a conclusion from it.
 */
#define HK_SWEEP_DWELL_MS 8000

/**
 * A longer first step that is NOT a data point.
 *
 * 315 Hz sits above a woofer's likely resonance and well below a tweeter's, so
 * for either driver it lands in the flat part of the curve where Z is close to
 * Re -- which makes it the right frequency to check the wiring against: the
 * reading should be a few millivolts, and the two failure modes are unmistakable
 * (about a volt means the driver is not connected and the meter is seeing the
 * whole source; about zero means it is shorted, or the mute lines never
 * released). Twenty seconds is enough to set a meter range, find the leads have
 * fallen off, and fix it before the data starts.
 *
 * Without this, the first two real steps get spent fiddling with the meter and
 * are lost, and they are at the bottom of the range where a woofer's curve is
 * already rising.
 */
#define HK_SWEEP_SETUP_K  140     /* 315.0 Hz */
#define HK_SWEEP_SETUP_MS 20000

/**
 * Silence before anything sounds, so there is time to act on the warning.
 *
 * The log says the amplifier must be out of the path. A log line that is
 * immediately followed by the thing it warns about is a log line that gets read
 * afterwards. Ten seconds is long enough to reach a power switch.
 */
#define HK_SWEEP_LEADIN_MS 10000

/** Blocks shorter than this make the DMA write loop churn for no benefit. */
#define HK_SWEEP_MIN_FRAMES 441

/*
 * The buffer holds the longest block any step can produce.
 *
 * For k >= HK_SWEEP_MIN_FRAMES the block is one cycle, so at most K_MAX frames.
 * For shorter cycles it is the smallest multiple of k that reaches
 * HK_SWEEP_MIN_FRAMES, which is strictly less than 2 * HK_SWEEP_MIN_FRAMES. The
 * assert is that arithmetic, so a future edit to either constant that breaks
 * the bound fails here rather than overrunning the array on step nineteen.
 */
#define HK_SWEEP_MAX_FRAMES HK_SWEEP_K_MAX
_Static_assert(HK_SWEEP_K_MAX >= 2 * HK_SWEEP_MIN_FRAMES,
               "hk_tone: the sweep block buffer is sized by the lowest frequency, and "
               "a short-cycle block can now be longer than that. Size it by the larger "
               "of the two.");

/** Stereo, interleaved, 16-bit: the format the DAC is configured for. */
static int16_t s_sweep[HK_SWEEP_MAX_FRAMES * 2];

/** A little over 4 kB of task stack: sinf, and log lines with several args. */
#define HK_SWEEP_TASK_STACK 4096

#if CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0
/** The frequencies of a fine pass, built once at startup. */
static uint16_t s_fine_k[HK_SWEEP_FINE_STEPS];
#endif

static unsigned clamp_k(uint32_t k)
{
    if (k < HK_SWEEP_K_MIN) {
        return HK_SWEEP_K_MIN;
    }
    if (k > HK_SWEEP_K_MAX) {
        return HK_SWEEP_K_MAX;
    }
    return (unsigned)k;
}

/** Tenths of a hertz, so the log needs no float formatting to be exact. */
static unsigned k_to_dhz(unsigned k)
{
    return (unsigned)((HK_TONE_RATE_HZ * 10u + k / 2u) / k);
}

/**
 * Fill the block with a whole number of cycles at 44100/k Hz.
 *
 * sinf() runs here, once per step -- twenty-two times in a three-minute run --
 * and never on the write path, for the same reason the fixed tone keeps it out:
 * a transcendental per sample would be a strange thing to add to a firmware
 * whose open question is whether samples arrive on time.
 *
 * The block begins and ends at phase zero, and that is what makes the steps
 * join cleanly. When the frequency changes, the underlying waveform passes
 * through zero from both sides: its slope changes, its value does not step. A
 * step WOULD be a broadband impulse, and putting twenty-two of those into an
 * unmeasured tweeter to save some arithmetic would be a poor trade.
 *
 * @return the number of frames written into s_sweep.
 */
static size_t fill_sweep_block(unsigned k)
{
    unsigned reps = 1;
    while (k * reps < HK_SWEEP_MIN_FRAMES) {
        reps++;
    }
    const unsigned frames = k * reps;

    for (unsigned i = 0; i < frames; i++) {
        const float phase = 2.0f * (float)M_PI * (float)reps
                          * (float)i / (float)frames;
        const int16_t sample = (int16_t)lrintf((float)HK_SWEEP_PEAK_LSB * sinf(phase));
        /* The same sample in both slots, as the fixed tone does: the operator
         * may have wired either channel to the series resistor, and a sweep
         * that came out of one channel would be indistinguishable from a dead
         * one. */
        s_sweep[2 * i]     = sample;
        s_sweep[2 * i + 1] = sample;
    }
    return frames;
}

static size_t fill_silence(void)
{
    for (unsigned i = 0; i < HK_SWEEP_MIN_FRAMES * 2u; i++) {
        s_sweep[i] = 0;
    }
    return HK_SWEEP_MIN_FRAMES;
}

/**
 * Hand the block to the DMA until the requested time has passed.
 *
 * The clock is the frame count, not a tick timer: i2s_channel_write() blocks
 * until the ring has room, so the bus itself paces this loop and the dwell is
 * exactly as long as the number of samples that left the pin. A vTaskDelay
 * would drift against the audio clock and, worse, would keep counting if the
 * bus stopped -- so a broken I2S peripheral would produce a sweep that looked
 * perfectly timed in the log and generated nothing.
 */
static esp_err_t play_for(size_t frames, uint32_t ms)
{
    const uint32_t target = (uint32_t)((uint64_t)HK_TONE_RATE_HZ * ms / 1000u);
    uint32_t done = 0;

    while (done < target) {
        size_t written = 0;
        const esp_err_t err = i2s_channel_write(s_tx, s_sweep,
                                               frames * 2u * sizeof(int16_t),
                                               &written, portMAX_DELAY);
        if (err != ESP_OK) {
            return err;
        }
        done += (uint32_t)(written / (2u * sizeof(int16_t)));
    }
    return ESP_OK;
}

/**
 * Build the fine pass around a centre frequency.
 *
 * Clamped to the k range, and steps that collapse onto the same k as their
 * neighbour are dropped: at the top of the range consecutive 1/12-octave
 * targets can round to the same integer number of frames per cycle, and holding
 * the identical frequency twice would put two rows in the operator's table that
 * cannot disagree. That is not extra data; it is a chance to misread the table.
 *
 * @return how many distinct steps were produced.
 */
#if CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0
static unsigned build_fine_pass(unsigned centre_hz)
{
    unsigned count = 0;

    for (unsigned i = 0; i < HK_SWEEP_FINE_STEPS; i++) {
        const uint32_t denom = (uint32_t)centre_hz * HK_SWEEP_FINE_RATIO_1024[i];
        const unsigned k =
            clamp_k((HK_TONE_RATE_HZ * 1024u + denom / 2u) / denom);
        if (count > 0 && s_fine_k[count - 1] == (uint16_t)k) {
            continue;
        }
        s_fine_k[count++] = (uint16_t)k;
    }
    return count;
}
#endif /* CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0 */

/**
 * Everything the operator needs before the first step, printed by the task that
 * is about to do it.
 *
 * The fixed tone prints its banner from hk_tone_start(), after the task exists,
 * so that a line which appears is a line that was true. The sweep cannot use
 * that arrangement: its lines have to arrive in a fixed order relative to the
 * lead-in silence and the steps, and two tasks racing to print would sometimes
 * put the "reset the board now" warning after the countdown it refers to. So
 * the task prints all of its own lines, and the first thing it does is print.
 */
static void log_sweep_banner(unsigned steps, unsigned first_k, unsigned last_k)
{
    const uint32_t total_ms = HK_SWEEP_LEADIN_MS + HK_SWEEP_SETUP_MS
                            + (uint32_t)steps * HK_SWEEP_DWELL_MS;

    ESP_LOGI(TAG, "STEPPED SWEEP for driver impedance and resonance (G0). "
                  "%d Hz, 16-bit stereo, bck gpio%d, ws gpio%d, data gpio%d",
             HK_TONE_RATE_HZ,
             HK_PIN_I2S_BCLK, HK_PIN_I2S_LRCLK, HK_PIN_I2S_DATA);

    ESP_LOGW(TAG, "THE AMPLIFIER MUST NOT BE IN THE PATH. Wire it: DAC line output -> "
                  "%d ohm 1/4 W series resistor -> ONE driver -> DAC ground, meter on "
                  "AC volts ACROSS THE DRIVER. Unplug the amplifier's input or cut its "
                  "supply first -- the two bench exception symbols that unmute the DAC "
                  "also unmute the amplifier, so there is no setting that keeps it out "
                  "and the wiring is the only thing that can.",
             HK_SWEEP_SERIES_OHM);

    ESP_LOGW(TAG, "ONE BARE DRIVER, nothing else in series with it. If the tweeter's "
                  "10 uF C_SAFE capacitor is already fitted, take it out of this circuit "
                  "-- at 40 Hz that capacitor is about 400 ohms and you would be "
                  "measuring it, not the voice coil. A curve that falls steadily from "
                  "the very first step instead of peaking is what that mistake looks "
                  "like.");

    ESP_LOGW(TAG, "amplitude %d dBFS (%d of 32767 counts, about half of full scale). "
                  "24 dB above the fixed tone, and that is the same safety rule applied "
                  "to a different circuit: behind %d ohms this is about 2.2 mA rms, "
                  "roughly 0.02 mW in a 4 ohm coil and under half a milliwatt at any "
                  "resonance peak these drivers can produce. Through the amplifier "
                  "instead it is clipping into an unmeasured tweeter.",
             HK_SWEEP_DBFS, HK_SWEEP_PEAK_LSB, HK_SWEEP_SERIES_OHM);

    ESP_LOGI(TAG, "%u steps, %u.%u Hz to %u.%u Hz, %u s each, after a %u s setup step: "
                  "about %u min %02u s in total. It runs to the end on its own -- no "
                  "button, no input. Write every reading down: the table to fill in is "
                  "in docs/02-hardware/driver-measurements.md.",
             steps,
             k_to_dhz(first_k) / 10u, k_to_dhz(first_k) % 10u,
             k_to_dhz(last_k) / 10u, k_to_dhz(last_k) % 10u,
             (unsigned)(HK_SWEEP_DWELL_MS / 1000u),
             (unsigned)(HK_SWEEP_SETUP_MS / 1000u),
             (unsigned)(total_ms / 60000u), (unsigned)((total_ms / 1000u) % 60u));

    ESP_LOGW(TAG, "SILENT for the next %u s. If the amplifier is still connected to the "
                  "DAC, or you do not know what is on the driver terminals, reset the "
                  "board NOW.",
             (unsigned)(HK_SWEEP_LEADIN_MS / 1000u));
}

static void log_sweep_result(unsigned steps)
{
    ESP_LOGI(TAG, "SWEEP COMPLETE: %u readings. The output is silent from here and I2S "
                  "keeps clocking, so a scope still sees the bus.", steps);
    ESP_LOGI(TAG, "Fs IS THE FREQUENCY OF THE LARGEST READING, not the smallest -- "
                  "impedance peaks at resonance. The smallest reading in the table is "
                  "where Z is closest to the DC resistance you already measured, so "
                  "Z(f) = Re x V(f) / Vmin. When a reading is more than about 5x Vmin, "
                  "correct the constant-current assumption: Z = Za x %d / (%d + Re - Za). "
                  "Expected values, what a second peak means and what this does NOT "
                  "close are in docs/02-hardware/driver-measurements.md.",
             HK_SWEEP_SERIES_OHM, HK_SWEEP_SERIES_OHM);
#if CONFIG_HK_BENCH_SWEEP_FINE_HZ == 0
    ESP_LOGI(TAG, "NEXT: set CONFIG_HK_BENCH_SWEEP_FINE_HZ to the peak frequency you just "
                  "found and reflash. That replaces this pass with %u steps of 1/12 "
                  "octave across +/- 2/3 octave around it, which is what pins Fs down to "
                  "a few percent. Nothing here is recorded as measured until you write "
                  "it into the doc.",
             (unsigned)HK_SWEEP_FINE_STEPS);
#endif
}

static void sweep_task(void *arg)
{
    (void)arg;

    const uint16_t *steps_k;
    unsigned        steps;

#if CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0
    steps   = build_fine_pass(CONFIG_HK_BENCH_SWEEP_FINE_HZ);
    steps_k = s_fine_k;
#else
    steps   = HK_SWEEP_COARSE_STEPS;
    steps_k = HK_SWEEP_COARSE_K;
#endif

    log_sweep_banner(steps, clamp_k(steps_k[0]), clamp_k(steps_k[steps - 1]));
#if CONFIG_HK_BENCH_SWEEP_FINE_HZ > 0
    ESP_LOGW(TAG, "FINE PASS around %d Hz. This is the second run: the coarse pass "
                  "already told you roughly where the peak is, and these %u steps say "
                  "where it actually is.",
             CONFIG_HK_BENCH_SWEEP_FINE_HZ, steps);
#endif

    esp_err_t err = play_for(fill_silence(), HK_SWEEP_LEADIN_MS);

    if (err == ESP_OK) {
        ESP_LOGW(TAG, "SETUP STEP, not a data point: %u.%u Hz for %u s. Set the meter to "
                      "AC volts -- the 200 mV range if it has a manual one -- and check "
                      "that you have a few millivolts. About 1 V means the driver is open "
                      "circuit and the meter is reading the whole source; about 0 mV "
                      "means it is shorted, or the mute lines never released and the "
                      "hk_audio lines above will say so.",
                 k_to_dhz(HK_SWEEP_SETUP_K) / 10u, k_to_dhz(HK_SWEEP_SETUP_K) % 10u,
                 (unsigned)(HK_SWEEP_SETUP_MS / 1000u));
        err = play_for(fill_sweep_block(HK_SWEEP_SETUP_K), HK_SWEEP_SETUP_MS);
    }

    for (unsigned i = 0; err == ESP_OK && i < steps; i++) {
        const unsigned k   = clamp_k(steps_k[i]);
        const unsigned dhz = k_to_dhz(k);
        /* Printed BEFORE the step sounds, because the operator is reading a
         * meter and needs to know which row this number belongs to. The count
         * is in the line for the same reason: a serial console scrolls, and
         * "step 14/22" is recoverable from a glance where a bare frequency is
         * not. */
        ESP_LOGI(TAG, "step %2u/%u   %5u.%u Hz   read the meter now (%u s)",
                 i + 1u, steps, dhz / 10u, dhz % 10u,
                 (unsigned)(HK_SWEEP_DWELL_MS / 1000u));
        err = play_for(fill_sweep_block(k), HK_SWEEP_DWELL_MS);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed mid-sweep: %s. The run is ABANDONED -- readings "
                      "taken before this point are still good, readings after it are of "
                      "nothing at all.",
                 esp_err_to_name(err));
    } else {
        log_sweep_result(steps);
    }

    /* The signal is over whether it finished or failed, so the caller is told
     * either way: the mute lines should come back down in both cases, and a
     * failed sweep that left the output chain released would be the worse of
     * the two outcomes. */
    signal_ended();

    /* Silence, forever, rather than exiting: the DMA keeps running and the bus
     * keeps clocking, so the board after a sweep looks on a scope exactly like
     * the board before one, and nobody has to work out whether a dead SCK is a
     * fault or just the end of the run. */
    while (true) {
        if (play_for(fill_silence(), 1000) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

#else /* !CONFIG_HK_BENCH_SWEEP */

/* ========================================================================= *
 *  MODE: FIXED TONE -- is there a working path from I2S to the speaker?
 * ========================================================================= */

/* --- The signal ---------------------------------------------------------- */

/**
 * 1 kHz. Near the middle of the woofer's range and nowhere near the crossover
 * region this project has not designed yet, so it is reproduced by whichever
 * driver happens to be connected and asks nothing of the one that is not.
 * It is also the frequency a person can identify as "a tone" rather than as
 * "a noise" from across a room, which is the entire measurement being taken.
 */
#define HK_TONE_HZ      1000

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
 *
 * CONFIG_HK_BENCH_SWEEP does run louder than this -- 24 dB louder -- and that
 * is not this limit being relaxed. It is a different circuit with no amplifier
 * in it, and it carries its own constant and its own assert.
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

#endif /* CONFIG_HK_BENCH_SWEEP */

/* --- Bring-up, shared by both modes --------------------------------------- */

esp_err_t hk_tone_start(hk_tone_done_fn on_done, void *context)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

#if CONFIG_HK_BENCH_SWEEP
    s_done         = on_done;
    s_done_context = context;
#else
    /* Taken and discarded. The fixed tone runs until the board is reset, so
     * there is no moment at which this could honestly be called, and holding a
     * pointer that will never be used would suggest otherwise. */
    (void)on_done;
    (void)context;
#endif

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

#if !CONFIG_HK_BENCH_SWEEP
    fill_block();
#endif

    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable: %s", esp_err_to_name(err));
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
        return err;
    }

#if CONFIG_HK_BENCH_SWEEP
    const BaseType_t created = xTaskCreatePinnedToCore(sweep_task, "hk_sweep",
                                                       HK_SWEEP_TASK_STACK, NULL,
                                                       HK_TONE_TASK_PRIO, &s_task,
                                                       HK_TONE_TASK_CORE);
#else
    const BaseType_t created = xTaskCreatePinnedToCore(tone_task, "hk_tone",
                                                       HK_TONE_TASK_STACK, NULL,
                                                       HK_TONE_TASK_PRIO, &s_task,
                                                       HK_TONE_TASK_CORE);
#endif
    if (created != pdPASS) {
        s_task = NULL;
        (void)i2s_channel_disable(s_tx);
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
        ESP_LOGE(TAG, "could not create the tone task");
        return ESP_ERR_NO_MEM;
    }

#if !CONFIG_HK_BENCH_SWEEP
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
#endif
    return ESP_OK;
}
