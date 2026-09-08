#include "hk_font.h"

#include <stddef.h>

#include "hk_gfx.h"

/*
 * U+FFFD. Nothing in any face maps it, so a malformed byte takes the same path
 * a Chinese character would: one space of advance and no ink. That is the whole
 * error policy, and it is deliberately the same policy for "this font is too
 * small to hold every script" and for "this stream is lying to us".
 */
#define REPLACEMENT 0xFFFDu

/* --- decoding --------------------------------------------------------------
 *
 * A track title arrives from the network. Nothing upstream of here has promised
 * it is well-formed UTF-8, or that it is UTF-8 at all, and the terminator is the
 * only thing keeping this loop inside the buffer. So two properties matter more
 * than decoding every legal sequence:
 *
 *   - the return value is at least 1 whenever the first byte is not NUL, so a
 *     caller's loop always moves forward;
 *   - a truncated sequence stops AT the NUL and reports only the bytes it
 *     actually consumed, so the caller never steps over the terminator.
 *
 * Overlong forms, surrogates and anything above U+10FFFF are rejected rather
 * than decoded. They are the shapes an attacker uses to smuggle one string past
 * a check and into another, and there is no reason for this panel to be the
 * component that accepts them.
 */

static bool is_continuation(char c)
{
    return ((unsigned char)c & 0xC0u) == 0x80u;
}

int hk_font_utf8_next(const char *utf8, uint32_t *codepoint)
{
    uint32_t cp = REPLACEMENT;
    int used = 1;

    if (utf8 == NULL || utf8[0] == '\0') {
        if (codepoint != NULL) {
            *codepoint = 0;
        }
        return 0;
    }

    const unsigned char b0 = (unsigned char)utf8[0];

    if (b0 < 0x80u) {
        cp = b0;
    } else if (b0 < 0xC2u || b0 > 0xF4u) {
        /* A stray continuation byte, or a lead byte that could only ever
         * introduce an overlong or an out-of-range codepoint. One byte, no
         * glyph, keep going. */
        cp = REPLACEMENT;
    } else {
        const int need = (b0 < 0xE0u) ? 1 : (b0 < 0xF0u) ? 2 : 3;
        uint32_t acc = b0 & (uint32_t)(0x7Fu >> (need + 1));
        int have = 0;
        while (have < need && is_continuation(utf8[1 + have])) {
            acc = (acc << 6) | ((uint32_t)(unsigned char)utf8[1 + have] & 0x3Fu);
            have++;
        }
        /* `have` counts only bytes that were really continuations, so a
         * sequence cut short by the terminator consumes what it read and
         * leaves the NUL where the caller will find it. */
        used = 1 + have;
        if (have < need) {
            cp = REPLACEMENT;
        } else if (acc >= 0xD800u && acc <= 0xDFFFu) {
            cp = REPLACEMENT;          /* a lone surrogate is not a character */
        } else if (acc < 0x80u || (need >= 2 && acc < 0x800u) ||
                   (need == 3 && acc < 0x10000u) || acc > 0x10FFFFu) {
            cp = REPLACEMENT;          /* overlong, or past the last plane */
        } else {
            cp = acc;
        }
    }

    if (codepoint != NULL) {
        *codepoint = cp;
    }
    return used;
}

/* --- the index -------------------------------------------------------------
 *
 * The generator emits `codepoints` sorted, which is the only reason this is a
 * binary search and not a scan of 172 entries per character.
 */

static const hk_glyph_t *glyph_for(const hk_face_t *face, uint32_t cp)
{
    if (cp > 0xFFFFu || face->count == 0) {
        return NULL;
    }
    const uint16_t needle = (uint16_t)cp;
    int lo = 0;
    int hi = (int)face->count - 1;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        const uint16_t here = face->codepoints[mid];
        if (here == needle) {
            return &face->glyphs[mid];
        }
        if (here < needle) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

/*
 * What an unknown codepoint costs. A space, because a missing letter should
 * leave a letter-sized hole rather than closing the word up or drawing a box.
 *
 * The fallback below only fires on a face with no U+0020, which the generator
 * cannot produce; it exists so a hand-edited table degrades into wide text
 * rather than into every glyph stacked in one column.
 */
static int space_advance(const hk_face_t *face)
{
    const hk_glyph_t *g = glyph_for(face, 0x20u);
    return (g != NULL) ? (int)g->advance : ((int)face->ascent + 2) / 4;
}

/* --- drawing --------------------------------------------------------------- */

static int mask_stride(const hk_glyph_t *g)
{
    return ((int)g->width + 1) / 2;   /* rows are padded to whole bytes */
}

/*
 * Scale one 0-255 quantity by another, ROUNDED.
 *
 * This has to be bit-for-bit what hk_gfx.c does when it multiplies a coverage
 * by an alpha, because the faded path below reaches the framebuffer a different
 * way than the plain path does and the two draw the same text. Truncating here
 * instead was worth 87 differing pixels on one line of a title -- invisible in
 * a still, and exactly the sort of thing that makes a marquee shimmer as a
 * glyph crosses from one path to the other.
 */
static uint8_t scale255(unsigned value, unsigned by)
{
    return (uint8_t)((value * by + 127u) / 255u);
}

/*
 * The faded path, one glyph.
 *
 * Columns outside, then rows inside: the fade factor depends only on x, so
 * this computes it once per column instead of once per pixel. The framebuffer
 * is walked against the grain as a result, which would matter for a full-screen
 * blit and does not for a glyph a dozen pixels wide.
 *
 * Everything here goes through hk_gfx_px() rather than hk_gfx_blit_a4(),
 * because blit_a4 takes one alpha for the whole mask and the whole point of
 * this function is that the alpha changes across it.
 */
static void blit_faded(uint16_t *buf, const hk_face_t *face, const hk_glyph_t *g,
                       int x, int y, uint16_t colour, uint8_t alpha,
                       int clip_x, int clip_w, int fade_left, int fade_right)
{
    const uint8_t *cov = &face->coverage[g->offset];
    const int stride = mask_stride(g);
    /* Widened before it is used, for the reason hk_gfx_clip_set() gives: a
     * caller who writes clip_w = INT_MAX means "the whole screen", and the
     * right-hand edge of that must not wrap round to the left of the glyph.
     * The distances below stay wide for the same reason -- they are only ever
     * multiplied by 255 after a comparison has bounded them, but the
     * comparison itself is against a number the caller chose. */
    const long long last = (long long)clip_x + (long long)clip_w - 1;

    for (int col = 0; col < (int)g->width; col++) {
        const int px = x + col;
        if (px < clip_x || (long long)px > last) {
            continue;
        }

        int ramp = 255;
        if (fade_left > 0) {
            const long long d = (long long)px - (long long)clip_x;
            if (d < (long long)fade_left) {
                ramp = (int)(d * 255 / fade_left);
            }
        }
        if (fade_right > 0) {
            const long long d = last - (long long)px;
            if (d < (long long)fade_right) {
                const int r = (int)(d * 255 / fade_right);
                if (r < ramp) {
                    ramp = r;
                }
            }
        }
        const unsigned a = scale255((unsigned)alpha, (unsigned)ramp);
        if (a == 0) {
            continue;
        }

        for (int row = 0; row < (int)g->height; row++) {
            const uint8_t byte = cov[row * stride + (col >> 1)];
            const uint8_t nib = ((col & 1) != 0) ? (uint8_t)(byte & 0x0Fu)
                                                 : (uint8_t)(byte >> 4);
            if (nib == 0) {
                continue;
            }
            /* 4 bits to 8 by replication, so 0xF becomes 0xFF and a full
             * pixel is actually full. Shifting alone would cap coverage at
             * 240/255 and leave every glyph faintly translucent. */
            const unsigned c8 = (unsigned)((nib << 4) | nib);
            hk_gfx_px(buf, px, y + row, colour, scale255(c8, a));
        }
    }
}

/*
 * One layout loop for all three entry points.
 *
 * hk_font_measure() has to return exactly what hk_font_draw() advances by --
 * a centred string is centred only if the two agree -- and the cheapest way to
 * guarantee that is to have one function that both of them are.
 */
static int run(uint16_t *buf, const hk_face_t *face, int x, int y,
               const char *utf8, uint16_t colour, uint8_t alpha,
               bool fade, int clip_x, int clip_w, int fade_left, int fade_right)
{
    if (face == NULL || utf8 == NULL) {
        return 0;
    }

    const int space = space_advance(face);
    int pen = 0;

    for (size_t i = 0;;) {
        uint32_t cp = 0;
        const int used = hk_font_utf8_next(&utf8[i], &cp);
        if (used <= 0) {
            break;
        }
        i += (size_t)used;

        const hk_glyph_t *g = glyph_for(face, cp);
        if (g == NULL) {
            pen += space;
            continue;
        }

        if (buf != NULL && g->width > 0 && g->height > 0) {
            const int gx = x + pen + g->left;
            const int gy = y + g->top;
            if (fade) {
                blit_faded(buf, face, g, gx, gy, colour, alpha,
                           clip_x, clip_w, fade_left, fade_right);
            } else {
                hk_gfx_blit_a4(buf, gx, gy, (int)g->width, (int)g->height,
                               &face->coverage[g->offset], mask_stride(g),
                               colour, alpha);
            }
        }
        pen += (int)g->advance;
    }
    return pen;
}

int hk_font_draw(uint16_t *buf, const hk_face_t *face, int x, int y,
                 const char *utf8, uint16_t colour, uint8_t alpha)
{
    return run(buf, face, x, y, utf8, colour, alpha, false, 0, 0, 0, 0);
}

int hk_font_measure(const hk_face_t *face, const char *utf8)
{
    return run(NULL, face, 0, 0, utf8, 0, 0, false, 0, 0, 0, 0);
}

int hk_font_draw_faded(uint16_t *buf, const hk_face_t *face, int x, int y,
                       const char *utf8, uint16_t colour, uint8_t alpha,
                       int clip_x, int clip_w, int fade_left, int fade_right)
{
    /* An empty clip rectangle draws nothing and still returns the advance: a
     * marquee that has scrolled its text fully out of view must not have its
     * layout collapse underneath it. */
    if (clip_w <= 0) {
        return hk_font_measure(face, utf8);
    }
    return run(buf, face, x, y, utf8, colour, alpha, true,
               clip_x, clip_w, fade_left, fade_right);
}
