/**
 * @file hk_sky.h
 * @brief The galaxy: the background every screen sits on.
 *
 * A barred spiral seen face-on, turning differentially -- the bulge faster than
 * the arms, the halo barely at all -- because a field of stars rotating as one
 * rigid disc reads as a spinning picture, and a real one does not do that.
 *
 * The star table is built once at init from a fixed seed and never allocated
 * again. Everything after that is a rotation and a brightness, so a frame costs
 * a walk over a table rather than a simulation. The nebula is a low-resolution
 * polar field, also built once, sampled through a precomputed pixel-to-polar
 * map: the alternative -- per-pixel trigonometry at 57,600 pixels a frame --
 * does not fit in the budget below.
 *
 * FRAME BUDGET. The panel is 240x240 at 16 bpp, so one transfer is 115,200
 * bytes; at 80 MHz that is 11.5 ms and nothing can make it shorter. 24 fps
 * leaves about 25 ms for everything else, and this file is the largest part of
 * it. hk_sky_render() reports its own cost through hk_sky_last_us() so the
 * number is measured rather than assumed.
 */
#ifndef HK_SKY_H
#define HK_SKY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * How the sky behaves, per screen.
 *
 * The same galaxy in four moods rather than four backgrounds: the pairing
 * screen wants it alive and turning, the playing screen wants it out of the
 * way, and a screen that changed its whole background would read as a
 * different device.
 */
typedef enum {
    HK_SKY_BOOT = 0,   /**< stars converge inward, then settle */
    HK_SKY_GALAXY,     /**< full brightness, full rotation -- pairing */
    HK_SKY_CALM,       /**< dimmed and slowed, so foreground text wins */
    HK_SKY_EMBER,      /**< warm-shifted, for charging */
    HK_SKY_ALERT,      /**< desaturated and still, for faults and holds */
    HK_SKY_MOOD_COUNT
} hk_sky_mood_t;

/**
 * Build the star table. Never freed.
 *
 * The cost is reported by hk_sky_init() itself rather than stated here: this
 * comment claimed "about 40 KB of PSRAM" and was wrong twice over -- the tables
 * are larger than that, and the file also holds about 14.5 KiB of .bss that
 * never leaves internal memory, which is the scarce kind on this part.
 *
 * @param seed  fixed, so every speaker shows the same sky and a screenshot
 *              taken today matches one taken next month.
 */
bool hk_sky_init(uint32_t seed);

/**
 * Draw the background into `buf`, overwriting it completely.
 *
 * @param t_ms   monotonic milliseconds; the only time input, so the same t
 *               always yields the same frame
 * @param mood   which behaviour
 * @param level  0-255 master brightness, for the iris transition
 */
void hk_sky_render(uint16_t *buf, uint32_t t_ms, hk_sky_mood_t mood, uint8_t level);

/**
 * The breathing value the rest of the UI shares, 0-255.
 *
 * Three oscillators at periods that share no common multiple, summed. The
 * screen should never look like it is repeating, and two combined periods
 * always eventually do.
 */
uint8_t hk_sky_breath(uint32_t t_ms);

/** Microseconds the last hk_sky_render() took. */
uint32_t hk_sky_last_us(void);

#endif /* HK_SKY_H */
