/**
 * @file hk_draw.h
 * @brief Drawing into a plain RGB565 buffer. No ESP-IDF underneath it.
 *
 * The panel is round, the buffer is square, and every primitive here clips.
 * That is the property worth testing rather than asserting: a write past the
 * end of a 115 200 byte framebuffer lands in whatever the allocator put next,
 * and the symptom would be a corrupted heap seconds later rather than a wrong
 * pixel now.
 *
 * Colours are RGB565 in the panel's own byte order, so the caller never has to
 * think about the wire format.
 */
#ifndef HK_DRAW_H
#define HK_DRAW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** The panel this project uses. Kept here so tests need no board. */
#define HK_DRAW_WIDTH 240
#define HK_DRAW_HEIGHT 240

/**
 * The circle the user can actually see.
 *
 * A 240x240 buffer on a round panel loses its corners -- 21.5% of the pixels
 * are never visible. Content stays inside this radius; only backgrounds go to
 * the edge.
 */
#define HK_DRAW_SAFE_R 104

/** Pack 8-bit components into RGB565. */
uint16_t hk_rgb(uint8_t r, uint8_t g, uint8_t b);

/** Scale a colour's brightness, 0-255. Used for breathing and dimming. */
uint16_t hk_rgb_scaled(uint8_t r, uint8_t g, uint8_t b, uint8_t level);

/** Every pixel. */
void hk_draw_fill(uint16_t *buf, uint16_t colour);

/** Filled circle, clipped. */
void hk_draw_disc(uint16_t *buf, int cx, int cy, int radius, uint16_t colour);

/**
 * Annulus between two radii, clipped. `inner` may exceed `outer`, in which case
 * nothing is drawn rather than the whole plane being filled.
 */
void hk_draw_ring(uint16_t *buf, int cx, int cy, int outer, int inner, uint16_t colour);

/**
 * Arc of an annulus, clockwise from twelve o'clock.
 *
 * `sweep` is in units of 1/1000 of a full turn, so a caller can express a
 * percentage without floating point in the render path.
 */
void hk_draw_arc(uint16_t *buf, int cx, int cy, int outer, int inner,
                 int sweep_per_mille, uint16_t colour);

/** Axis-aligned rectangle, clipped. */
void hk_draw_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t colour);

/**
 * One seven-segment glyph, top-left at (x, y).
 *
 * Seven segments rather than a bitmap font: the whole set this screen needs is
 * hex digits and a handful of marks, and a stroke form scales to any size
 * without a second table. Unknown characters draw nothing, so a caller cannot
 * turn a typo into a wrong reading.
 *
 * Returns the advance in pixels, so a caller can lay out a string without
 * knowing the geometry.
 */
int hk_draw_glyph(uint16_t *buf, int x, int y, int w, int h, int thickness,
                  char ch, uint16_t colour);

/** A string of glyphs. Returns the total advance. */
int hk_draw_text(uint16_t *buf, int x, int y, int w, int h, int thickness,
                 const char *text, uint16_t colour);

/**
 * Width a string would occupy, without drawing it.
 *
 * Takes the same geometry hk_draw_text() does, so a centred string is actually
 * centred. Deriving the thickness here instead would make the two disagree
 * whenever a caller passed a different one.
 */
int hk_draw_text_width(int w, int thickness, int count);

#endif /* HK_DRAW_H */
