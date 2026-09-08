/**
 * @file hk_gfx.h
 * @brief Compositing over the RGB565 framebuffer: alpha, antialiasing, clipping.
 *
 * hk_draw.h is the hard-edged layer -- discs, rings, rectangles, seven-segment
 * glyphs -- and it stays, because the self test and the fallback screens want
 * exactly that and nothing more. This is the layer the designed UI needs: every
 * primitive here takes an alpha, every edge is antialiased, and everything
 * obeys a clip rectangle so a marquee can scroll text out of its own box
 * instead of over the artist's name.
 *
 * Subpixel geometry is in 1/16 px fixed point ("_q4" suffix), not float. The
 * render path runs at 24 fps on a part whose FPU is single-precision and whose
 * framebuffer lives in PSRAM; the arithmetic here is the cheap part and it
 * stays integer so it cannot become the expensive part.
 *
 * Alpha is 0-255 and composites source-over. RGB565 has 5 or 6 bits per
 * channel, so repeated blending of the same faint colour quantises rather than
 * accumulating -- draw a translucent shape once, not as ten passes.
 */
#ifndef HK_GFX_H
#define HK_GFX_H

#include <stdbool.h>
#include <stdint.h>

#include "hk_draw.h"

/** Convert whole pixels to the 1/16 px fixed point the shape calls take. */
#define HK_Q4(px) ((int)((px) * 16))

/**
 * Clip rectangle. Every primitive IN THIS FILE intersects with it before
 * touching memory -- and only this file.
 *
 * hk_draw.c's primitives clip to the framebuffer and know nothing about this
 * rectangle, so hk_draw_text() inside a clip will draw straight through its
 * edge. That is not a defect in either file; they are two layers with two
 * jobs. It is a trap for a caller who assumes clipping is a property of the
 * target, so: text that must be cut off goes through hk_font, not hk_draw.
 *
 * Clipping is a property of the target, not of each call, because the caller
 * that needs it -- a marquee, a scrolling list -- needs it to apply to a whole
 * group of draws. Set it, draw, reset it.
 */
void hk_gfx_clip_set(int x, int y, int w, int h);
void hk_gfx_clip_reset(void);

/** Source-over composite of two RGB565 values. */
uint16_t hk_gfx_blend(uint16_t dst, uint16_t src, uint8_t alpha);

/** Linear interpolation between two RGB565 values; t is 0-255. */
uint16_t hk_gfx_mix(uint16_t a, uint16_t b, uint8_t t);

/** One pixel, clipped and blended. The single place that writes to `buf`. */
void hk_gfx_px(uint16_t *buf, int x, int y, uint16_t colour, uint8_t alpha);

/** Antialiased filled circle. Centre and radius in 1/16 px. */
void hk_gfx_disc(uint16_t *buf, int cx_q4, int cy_q4, int r_q4,
                 uint16_t colour, uint8_t alpha);

/** Antialiased annulus. `inner_q4 >= outer_q4` draws nothing. */
void hk_gfx_ring(uint16_t *buf, int cx_q4, int cy_q4, int outer_q4, int inner_q4,
                 uint16_t colour, uint8_t alpha);

/**
 * Antialiased arc of an annulus.
 *
 * Angles are in 1/1000 of a full turn, measured clockwise from twelve o'clock,
 * so a progress ring is `sweep = 1000 * elapsed / duration` with no trig and no
 * degrees-versus-radians mistake available to make.
 */
void hk_gfx_arc(uint16_t *buf, int cx_q4, int cy_q4, int outer_q4, int inner_q4,
                int start_per_mille, int sweep_per_mille,
                uint16_t colour, uint8_t alpha);

/** Rounded rectangle, filled. Whole pixels; `radius` is clamped to fit. */
void hk_gfx_rrect(uint16_t *buf, int x, int y, int w, int h, int radius,
                  uint16_t colour, uint8_t alpha);

/** Rounded rectangle, outline of the given width, drawn inside the bounds. */
void hk_gfx_rrect_outline(uint16_t *buf, int x, int y, int w, int h, int radius,
                          int stroke, uint16_t colour, uint8_t alpha);

/** Axis-aligned rectangle with alpha. */
void hk_gfx_rect(uint16_t *buf, int x, int y, int w, int h,
                 uint16_t colour, uint8_t alpha);

/**
 * Blit an 8-bit coverage mask as a solid colour.
 *
 * This is how glyphs and icons reach the screen: the mask says how much of each
 * pixel the shape covers, the colour says what it is. `alpha` scales the whole
 * mask, which is what makes a label fade without re-rasterising it.
 */
void hk_gfx_blit_a8(uint16_t *buf, int x, int y, int w, int h,
                    const uint8_t *coverage, int stride,
                    uint16_t colour, uint8_t alpha);

/**
 * Blit a 4-bit coverage mask, two pixels per byte, high nibble first.
 *
 * The fonts ship in this form: 4 bits of coverage is visually indistinguishable
 * from 8 at these sizes and halves the flash they occupy.
 */
void hk_gfx_blit_a4(uint16_t *buf, int x, int y, int w, int h,
                    const uint8_t *coverage, int stride_bytes,
                    uint16_t colour, uint8_t alpha);

/**
 * Darken everything outside a radius, smoothly.
 *
 * The panel is round and its bezel is black; a background that simply stops at
 * the edge reads as a rectangle behind a hole. This is what makes the image sit
 * in the glass instead of being pasted onto it.
 */
void hk_gfx_vignette(uint16_t *buf, int cx, int cy, int inner_r, int outer_r,
                     uint8_t strength);

/**
 * Scale every pixel's brightness by `level` (0-255) inside a radius.
 *
 * The iris transition: two screens are drawn one after the other and the
 * outgoing one is collapsed rather than cross-faded, because a cross-fade of
 * two bright screens goes through a bright middle and reads as a flash.
 */
void hk_gfx_dim(uint16_t *buf, uint8_t level);

/**
 * Keep only what is inside `radius`, fading the boundary, and fill the rest
 * with `outside`. This is the iris itself.
 */
void hk_gfx_iris(uint16_t *buf, int cx, int cy, int radius_q4, int feather_q4,
                 uint16_t outside, bool keep_inside);

#endif /* HK_GFX_H */
