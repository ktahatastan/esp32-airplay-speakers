/**
 * @file hk_audio_hw.h
 * @brief The ESP-IDF half of hk_audio: two GPIOs and a clock to move them with.
 *
 * hk_audio.c is a pure state machine with no globals and no pins, so it can be
 * driven through a hundred start-up and shutdown cycles in a host test in
 * microseconds. That is why it is worth having, and it is also why, on its own,
 * it does nothing at all: until 2026-09-08 nothing in this firmware called
 * hk_audio_step() and nothing ever configured HK_PIN_AMP_MUTE or
 * HK_PIN_DAC_XSMT, so the two mute lines sat at whatever their external
 * pull-downs decided, forever. This file is the missing half.
 *
 * It owns one hk_audio_t, ticks it in its own low-priority task, and writes
 * hk_audio_outputs() onto the two pins. That is the whole job.
 *
 * WHAT IT DELIBERATELY DOES NOT DO
 *
 * It does not touch I2S. hk_audio_outputs() reports an `i2s_running` line
 * because the sequence is written in terms of all three, but on this board the
 * clocks belong to the vendored AirPlay receiver: it configures the I2S
 * peripheral when it starts and clocks it for as long as it is running. So
 * `i2s_running` is read here as an observation, not as a command, and the
 * receiver's own "playing" callback is what feeds ::hk_audio_hw_set_stream_live.
 * A second owner reaching for the same peripheral is how a subsystem ends up
 * fighting another one for hardware it does not own.
 *
 * It does not touch samples either. There is no gain, no attenuation and no
 * ceiling in this file, because there is nothing here that could honestly set
 * one: the crossover, the protective high-pass and the limiter are firmware
 * stage F3, and F3 waits on the G0 driver-impedance measurement that is still
 * an open blocker. A number invented here would be indistinguishable from a
 * measured one, which is the failure mode this whole repository is arranged
 * against.
 *
 * MUTED IS THE RESTING STATE, and the mechanism is the one hk_pins.h describes:
 * both lines are active low against external pull-downs, so the safe level is
 * the level the pad already has at reset. This module only ever RELEASES mute.
 * If it crashes, if the task is deleted, if the chip resets, if this firmware
 * never runs at all — the pads go high-impedance, the pull-downs win and the
 * speakers are quiet.
 *
 * NOT YET VERIFIED ON HARDWARE. This compiles against ESP-IDF v5.5.1. No board
 * has been watched on a scope while it ran, and the settle times it supplies
 * are provisional: see the note on them in hk_audio_hw.c.
 */
#ifndef HK_AUDIO_HW_H
#define HK_AUDIO_HW_H

#include <stdbool.h>

#include "esp_err.h"

/**
 * Drive both mute lines to their safe level and start the sequencer task.
 *
 * The pins are asserted (muted) before the task exists, and the sequencer
 * starts in ::HK_AUDIO_SILENT with permission assumed absent, so the earliest
 * possible moment at which anything could be released is one tick after a
 * caller has said otherwise.
 *
 * The settle times this uses are chosen in hk_audio_hw.c rather than exposed
 * here. hk_audio.h leaves hk_audio_timing_t without defaults on purpose, and a
 * board layer publishing its guesses as constants other code could reach for
 * would quietly undo that. They are logged at start instead, so the boot record
 * carries the numbers that were actually used.
 *
 * @return ESP_ERR_INVALID_STATE if already started, ESP_ERR_NO_MEM if the task
 *         could not be created, otherwise whatever gpio_config() returned.
 */
esp_err_t hk_audio_hw_start(void);

/**
 * Whether sound is allowed at all right now.
 *
 * The composition of that answer — a calibration profile exists, the pack and
 * the charger allow it, and any bench exception that has been deliberately
 * configured — belongs to the application, not here. This module is told the
 * conclusion so that it stays a hardware layer with no policy in it, and so
 * that the one place the exceptions are applied is the one place they are
 * printed.
 *
 * False at start, and false is the only value this module will assume for
 * itself. Safe to call from any task.
 */
void hk_audio_hw_set_permitted(bool permitted);

/**
 * Whether the receiver is delivering audio.
 *
 * Fed from the AirPlay receiver's playback callback, which runs on the RTSP
 * task: this call does nothing but store a bool, which is all that task should
 * be asked to pay for. Safe to call from any task.
 */
void hk_audio_hw_set_stream_live(bool live);

#endif /* HK_AUDIO_HW_H */
