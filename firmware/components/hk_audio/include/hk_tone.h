/**
 * @file hk_tone.h
 * @brief A known signal on the I2S pins, so the receiver stops being a suspect.
 *
 * WHY THIS EXISTS
 *
 * On 2026-09-08 the bench board reached the state where AirPlay metadata
 * appears on the screen -- so the RTSP session, the network and the display all
 * work -- and no sound comes out. From the device's side those two facts are
 * compatible with two completely different faults:
 *
 *   1. the receiver is not delivering samples to I2S at all, or is delivering
 *      silence, in which case everything after the ESP32 is innocent; or
 *   2. samples are leaving GPIO6 exactly as they should and the DAC, the
 *      amplifier, the wiring or the mute lines are eating them.
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
 *                        SILENT, the mute lines are still asserted and this
 *                        module was never the problem.
 *
 * WHAT IT DELIBERATELY DOES NOT DO
 *
 * It does not coexist with the AirPlay receiver. The vendored receiver calls
 * i2s_new_channel() on I2S_NUM_0 and owns that channel for as long as it runs;
 * so does this. Two owners of one channel is a worse and less legible bug than
 * the one being chased, so hk_main starts one or the other and never both --
 * see CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY.
 *
 * It does not touch the mute lines. Those belong to hk_audio_hw and to the
 * sequence in hk_audio.c, and releasing them is still gated on the same two
 * bench exceptions as before. A tone generated into an asserted mute is
 * inaudible and that is correct: this module is an instrument, not an override.
 *
 * NOT A PRODUCT FEATURE. The source is only compiled when the bench symbol is
 * set, so a release image does not carry it.
 */
#ifndef HK_TONE_H
#define HK_TONE_H

#include "esp_err.h"

/**
 * Configure I2S on the hk_pins audio pins and start writing the tone.
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
 * a tone that produces another round of questions.
 *
 * @return ESP_ERR_INVALID_STATE if already started, ESP_ERR_NO_MEM if the task
 *         could not be created, otherwise whatever the I2S driver returned.
 */
esp_err_t hk_tone_start(void);

#endif /* HK_TONE_H */
