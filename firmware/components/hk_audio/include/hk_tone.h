/**
 * @file hk_tone.h
 * @brief A known signal on the I2S pins, so the receiver stops being a suspect.
 *
 * WHY THIS EXISTS
 *
 * On 2026-09-08 the bench board reached the state where AirPlay metadata
 * appears in the log -- so the RTSP session and the network work -- and no
 * sound comes out. From the device's side those two facts are compatible with
 * two completely different faults:
 *
 *   1. the receiver is not delivering samples to I2S at all, or is delivering
 *      silence, in which case everything after the ESP32 is innocent; or
 *   2. samples are leaving GPIO6 exactly as they should and the DAC, the
 *      amplifier, the wiring or the DAC mute are eating them.
 *
 * Nothing in the log separates those, because the firmware's only source of
 * audio is a vendored receiver whose internals are not this project's to
 * instrument. This module removes the receiver from the question: it puts a
 * signal this file generated, at an amplitude this file chose, onto the same
 * three pins, and lets the operator listen.
 *
 *   A clean tone      -> everything from the I2S peripheral to the speaker is
 *                        good. The fault is upstream, in the receive/decode
 *                        path, and hk_airplay is where to look next.
 *   Noise, no tone    -> the bus is clocking and the analogue chain is wrong:
 *                        DAC power, the SCK-to-ground strap, the data pin, the
 *                        amplifier input or its ground.
 *   Silence           -> nothing is reaching the amplifier. Read the hk_audio
 *                        transition lines first: if the sequence never left
 *                        SILENT, the DAC mute is still asserted and this
 *                        module was never the problem.
 *
 * TWO SIGNALS, AND THEY ARE NOT THE SAME INSTRUMENT
 *
 * The paragraphs above describe the FIXED TONE: 1 kHz at -30 dBFS, held
 * forever, listened to through the DAC and the amplifier and a speaker. It
 * answers "does the chain carry audio at all".
 *
 * CONFIG_HK_BENCH_SWEEP replaces it with a STEPPED SINE SWEEP, and that is a
 * different instrument answering a different question: what is the impedance
 * curve of one Nova driver, and where is its resonance. It is the G0 blocker
 * in AGENTS.md, and until it is measured the tweeter's protective high-pass
 * corner is a guess -- the corner has to sit at least an octave above the
 * tweeter's Fs, and nobody knows what Fs is.
 *
 * The two differ in EVERY respect that matters, which is why they are two
 * compile-time modes and not one runtime switch:
 *
 *                        fixed tone                 stepped sweep
 *   what is connected    DAC -> amplifier ->        DAC line out -> a 470 ohm
 *                        speaker                    series resistor -> ONE
 *                                                   driver -> DAC ground.
 *                                                   NO AMPLIFIER.
 *   amplitude            -30 dBFS                   -6 dBFS
 *   what reads it        a person's ear             a multimeter on AC volts
 *   how long             forever                    about three and a half
 *                                                   minutes, then silence
 *
 * The amplitude difference is 24 dB and it is not a relaxation of the safety
 * rule -- it is the same rule applied to a different circuit. -30 dBFS is
 * chosen for what sits downstream of the fixed tone: a power amplifier of
 * unknown, possibly 36 dB gain, feeding an unmeasured driver with no crossover,
 * no high-pass and no limiter, because the tone is written straight to I2S
 * and never passes through hk_dsp. The sweep has none of that downstream: 470 ohms
 * and a voice coil, so at -6 dBFS the driver dissipates tens of microwatts and,
 * at the worst point of a resonance peak, under half a milliwatt.
 *
 * THE COROLLARY, WHICH IS THE DANGEROUS PART. Nothing in this firmware can see
 * how the bench is wired. If the sweep is run with the TPA3110 still fed from
 * the DAC, -6 dBFS through 36 dB of gain is far past that amplifier's clipping
 * point into a 4 ohm load, and it is being held near a tweeter's resonance for
 * eight seconds at a time. The amplifier must be out of the path PHYSICALLY --
 * its input unplugged or its supply off -- and the module says so in the log
 * and then stays silent for ten seconds before the first step so there is time
 * to act on it. See docs/02-hardware/driver-measurements.md for the circuit.
 *
 * WHAT IT DELIBERATELY DOES NOT DO
 *
 * It does not coexist with the AirPlay receiver. The vendored receiver calls
 * i2s_new_channel() on I2S_NUM_0 and owns that channel for as long as it runs;
 * so does this. Two owners of one channel is a worse and less legible bug than
 * the one being chased, so hk_main starts one or the other and never both --
 * see CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY.
 *
 * It does not touch the DAC mute. That belongs to hk_audio_hw and to the
 * sequence in hk_audio.c, and releasing it is still gated on the same two
 * bench exceptions as before. A tone generated into an asserted mute is
 * inaudible and that is correct: this module is an instrument, not an override.
 *
 * There is an unpleasant consequence of that for the sweep specifically, and it
 * is written here rather than only in the doc because it is the one thing an
 * operator would not guess. Releasing the DAC's XSMT so the sweep reaches the
 * line output requires the two bench exception symbols -- and the amplifiers
 * have no mute of their own, so a DAC that is unmuted is an amplifier that is
 * amplifying. There is no setting that unmutes the DAC and holds the
 * amplifier down, because there is no line to hold it down with. That is why
 * "the amplifier is not in the path" has to be true of the wiring and cannot
 * be made true of the configuration.
 *
 * NOT A PRODUCT FEATURE. The source is only compiled when the bench symbol is
 * set, so a release image does not carry it.
 */
#ifndef HK_TONE_H
#define HK_TONE_H

#include "esp_err.h"

/**
 * Called once when the generated signal has ended.
 *
 * The fixed tone never ends, so in that build this is never called and that is
 * not a defect -- it is the difference between the two modes stated in the type
 * system rather than in a comment. The sweep ends after its last step, and the
 * caller's business at that moment is to stop claiming a live stream so the
 * mute sequence puts the output chain back down.
 *
 * Runs on the generator task, not the caller's. Do only what the AirPlay
 * playback callback does: store a value.
 */
typedef void (*hk_tone_done_fn)(void *context);

/**
 * Configure I2S on the hk_pins audio pins and start writing the signal.
 *
 * Takes exclusive ownership of I2S_NUM_0 for the life of the process. The
 * caller is responsible for making sure the AirPlay receiver is not also
 * started; there is no arbitration here, because the correct number of owners
 * is one and a module that negotiated for the peripheral would be pretending
 * otherwise.
 *
 * Everything the operator needs in order to act on the result -- frequency,
 * amplitude in dBFS and as a fraction of full scale, sample rate, the three
 * pins, and what each of the three possible outcomes means -- is logged here.
 * That log is half of what this module is for: a tone nobody can interpret is
 * a tone that produces another round of questions. In sweep builds the log is
 * more than half of it: the frequency of each step is printed BEFORE that step
 * starts sounding, because the operator is reading a meter and has to know
 * which row of the table the number in front of them belongs to.
 *
 * @param on_done Called once when the signal ends; never, for the fixed tone.
 *                May be NULL.
 * @param context Passed through to @p on_done untouched.
 *
 * @return ESP_ERR_INVALID_STATE if already started, ESP_ERR_NO_MEM if the task
 *         could not be created, otherwise whatever the I2S driver returned.
 */
esp_err_t hk_tone_start(hk_tone_done_fn on_done, void *context);

#endif /* HK_TONE_H */
