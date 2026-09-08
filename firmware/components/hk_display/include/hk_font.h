/**
 * @file hk_font.h
 * @brief Proportional antialiased text, in three faces.
 *
 * hk_draw's seven-segment glyphs can write a device id and a PIN and nothing
 * else. A track called "Nihavent Longa" needs letters, and it needs them in
 * Turkish, so the coverage here is Latin-1 plus the six Turkish pairs that are
 * not in it: C-cedilla, G-breve, dotless I, dotted I, S-cedilla, and the
 * o/u diaereses that Latin-1 already has.
 *
 * Glyphs are 4-bit coverage masks, two pixels per byte. They are generated, not
 * hand-drawn -- see tools/gen_font.py, which records which face and which size
 * each table came from, so the next person can regenerate rather than patch
 * pixels.
 *
 * Text arrives from AirPlay as UTF-8 and is decoded here. A byte sequence this
 * font has no glyph for advances by the width of a space rather than drawing a
 * box: a missing letter should cost the reader a letter, not a word.
 */
#ifndef HK_FONT_H
#define HK_FONT_H

#include <stdbool.h>
#include <stdint.h>

/** One glyph's placement and its coverage mask. */
typedef struct {
    uint16_t offset;      /**< byte offset into the face's coverage blob */
    uint8_t  width;       /**< mask width in pixels */
    uint8_t  height;      /**< mask height in pixels */
    int8_t   left;        /**< pen-relative x of the mask's left edge */
    int8_t   top;         /**< baseline-relative y of the mask's top edge */
    uint8_t  advance;     /**< pen movement, in pixels */
} hk_glyph_t;

/** A face: one size of one typeface. */
typedef struct {
    const char       *name;
    uint8_t           ascent;      /**< baseline to top, for vertical centring */
    uint8_t           descent;
    uint8_t           line_height;
    uint16_t          first;       /**< first codepoint in the index */
    uint16_t          count;       /**< entries in the index */
    const uint16_t   *codepoints;  /**< sorted; binary searched */
    const hk_glyph_t *glyphs;      /**< parallel to codepoints */
    const uint8_t    *coverage;    /**< 4bpp masks, two pixels per byte */
} hk_face_t;

/**
 * The three faces.
 *
 * Small for chips and labels, body for the artist and the time, display for
 * the track title and the device name. A fourth size would be a fourth table
 * in flash for a difference nobody at arm's length can see.
 */
const hk_face_t *hk_font_small(void);
const hk_face_t *hk_font_body(void);
const hk_face_t *hk_font_display(void);

/**
 * Draw UTF-8 text with its left edge at `x` and its BASELINE at `y`.
 *
 * Baseline rather than top, because two strings in different faces line up on
 * their baselines and not on their tops. Returns the advance in pixels.
 */
int hk_font_draw(uint16_t *buf, const hk_face_t *face, int x, int y,
                 const char *utf8, uint16_t colour, uint8_t alpha);

/** Advance the same string would take, without drawing it. */
int hk_font_measure(const hk_face_t *face, const char *utf8);

/**
 * Draw with a per-glyph alpha ramp, for text that fades into a boundary.
 *
 * `fade_left` and `fade_right` are pixel widths measured inward from the clip
 * rectangle. A marquee whose text simply stops at the edge reads as broken;
 * one that dissolves reads as continuing.
 */
int hk_font_draw_faded(uint16_t *buf, const hk_face_t *face, int x, int y,
                       const char *utf8, uint16_t colour, uint8_t alpha,
                       int clip_x, int clip_w, int fade_left, int fade_right);

/** Decode one UTF-8 sequence. Returns bytes consumed; 0 at the terminator. */
int hk_font_utf8_next(const char *utf8, uint32_t *codepoint);

#endif /* HK_FONT_H */
