#include "hk_draw.h"

/* Every write goes through here. One clip, one place to get it right. */
static inline void put(uint16_t *buf, int x, int y, uint16_t colour)
{
    if (x < 0 || y < 0 || x >= HK_DRAW_WIDTH || y >= HK_DRAW_HEIGHT) {
        return;
    }
    buf[(size_t)y * HK_DRAW_WIDTH + (size_t)x] = colour;
}

uint16_t hk_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                      ((uint16_t)(g & 0xFC) << 3) |
                      ((uint16_t)(b) >> 3));
}

uint16_t hk_rgb_scaled(uint8_t r, uint8_t g, uint8_t b, uint8_t level)
{
    return hk_rgb((uint8_t)((unsigned)r * level / 255u),
                  (uint8_t)((unsigned)g * level / 255u),
                  (uint8_t)((unsigned)b * level / 255u));
}

void hk_draw_fill(uint16_t *buf, uint16_t colour)
{
    if (buf == NULL) {
        return;
    }
    for (size_t i = 0; i < (size_t)HK_DRAW_WIDTH * HK_DRAW_HEIGHT; i++) {
        buf[i] = colour;
    }
}

void hk_draw_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t colour)
{
    if (buf == NULL || w <= 0 || h <= 0) {
        return;
    }
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            put(buf, col, row, colour);
        }
    }
}

/*
 * Discs and rings are drawn by scanline with a squared-radius test: no sqrt in
 * the render path, and the comparison is exact in integers, so the edge is the
 * same every frame rather than shimmering with rounding.
 */
void hk_draw_disc(uint16_t *buf, int cx, int cy, int radius, uint16_t colour)
{
    if (buf == NULL || radius <= 0) {
        return;
    }
    const int rr = radius * radius;
    for (int y = cy - radius; y <= cy + radius; y++) {
        const int dy = y - cy;
        for (int x = cx - radius; x <= cx + radius; x++) {
            const int dx = x - cx;
            if (dx * dx + dy * dy <= rr) {
                put(buf, x, y, colour);
            }
        }
    }
}

void hk_draw_ring(uint16_t *buf, int cx, int cy, int outer, int inner, uint16_t colour)
{
    /* An inner radius at or beyond the outer one is an empty ring, not a filled
     * plane. Getting this backwards would paint the whole screen. */
    if (buf == NULL || outer <= 0 || inner >= outer) {
        return;
    }
    if (inner < 0) {
        inner = 0;
    }
    const int ro = outer * outer;
    const int ri = inner * inner;
    for (int y = cy - outer; y <= cy + outer; y++) {
        const int dy = y - cy;
        for (int x = cx - outer; x <= cx + outer; x++) {
            const int dx = x - cx;
            const int d = dx * dx + dy * dy;
            if (d <= ro && d >= ri) {
                put(buf, x, y, colour);
            }
        }
    }
}

/*
 * Angle without trigonometry.
 *
 * Only the ORDER of angles matters for an arc, so a monotone function of the
 * angle is enough. This uses the octant plus a linear interpolation inside it,
 * which is monotone, exact in integers, and within about one degree of the true
 * angle -- far finer than the eye reads on a 104 px radius.
 */
static int angle_per_mille(int dx, int dy)
{
    /* Screen y grows downward; negate so twelve o'clock is zero. */
    const int x = dx;
    const int y = -dy;
    const int ax = x < 0 ? -x : x;
    const int ay = y < 0 ? -y : y;
    if (ax == 0 && ay == 0) {
        return 0;
    }

    int octant;      /* eighths of a turn, clockwise from twelve */
    int num, den;
    if (y >= 0 && x >= 0) {
        if (x <= y) { octant = 0; num = ax; den = ay; }
        else        { octant = 1; num = ay; den = ax; }
    } else if (y < 0 && x >= 0) {
        if (ax >= ay) { octant = 2; num = ay; den = ax; }
        else          { octant = 3; num = ax; den = ay; }
    } else if (y < 0 && x < 0) {
        if (ax <= ay) { octant = 4; num = ax; den = ay; }
        else          { octant = 5; num = ay; den = ax; }
    } else {
        if (ax >= ay) { octant = 6; num = ay; den = ax; }
        else          { octant = 7; num = ax; den = ay; }
    }
    /* Odd octants run backwards through the wedge. */
    const int within = (octant % 2 == 0) ? (num * 125 / den) : (125 - num * 125 / den);
    return octant * 125 + within;
}

void hk_draw_arc(uint16_t *buf, int cx, int cy, int outer, int inner,
                 int sweep_per_mille, uint16_t colour)
{
    if (buf == NULL || outer <= 0 || inner >= outer || sweep_per_mille <= 0) {
        return;
    }
    if (sweep_per_mille > 1000) {
        sweep_per_mille = 1000;
    }
    if (inner < 0) {
        inner = 0;
    }
    const int ro = outer * outer;
    const int ri = inner * inner;
    for (int y = cy - outer; y <= cy + outer; y++) {
        const int dy = y - cy;
        for (int x = cx - outer; x <= cx + outer; x++) {
            const int dx = x - cx;
            const int d = dx * dx + dy * dy;
            if (d > ro || d < ri) {
                continue;
            }
            if (angle_per_mille(dx, dy) <= sweep_per_mille) {
                put(buf, x, y, colour);
            }
        }
    }
}

/*
 * Seven segments, bit per segment:
 *   0 top      1 top-right  2 bottom-right  3 bottom
 *   4 bottom-left  5 top-left  6 middle
 */
#define SEG_A 0x01u
#define SEG_B 0x02u
#define SEG_C 0x04u
#define SEG_D 0x08u
#define SEG_E 0x10u
#define SEG_F 0x20u
#define SEG_G 0x40u

static unsigned segments_for(char ch)
{
    switch (ch) {
    case '0': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    case '1': return SEG_B | SEG_C;
    case '2': return SEG_A | SEG_B | SEG_G | SEG_E | SEG_D;
    case '3': return SEG_A | SEG_B | SEG_G | SEG_C | SEG_D;
    case '4': return SEG_F | SEG_G | SEG_B | SEG_C;
    case '5': return SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
    case '6': return SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D;
    case '7': return SEG_A | SEG_B | SEG_C;
    case '8': return 0x7Fu;
    case '9': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
    case 'A': case 'a': return SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    case 'B': case 'b': return SEG_F | SEG_E | SEG_D | SEG_C | SEG_G;
    case 'C': case 'c': return SEG_A | SEG_F | SEG_E | SEG_D;
    case 'D': case 'd': return SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    case 'E': case 'e': return SEG_A | SEG_F | SEG_G | SEG_E | SEG_D;
    case 'F': case 'f': return SEG_A | SEG_F | SEG_G | SEG_E;
    case '-':           return SEG_G;
    case '_':           return SEG_D;
    case ' ':           return 0u;
    default:            return 0u;   /* unknown draws nothing, never a wrong glyph */
    }
}

int hk_draw_glyph(uint16_t *buf, int x, int y, int w, int h, int t,
                  char ch, uint16_t colour)
{
    if (w <= 0 || h <= 0 || t <= 0) {
        return 0;
    }
    const unsigned segs = segments_for(ch);
    if (buf != NULL && segs != 0u) {
        const int half = h / 2;
        if (segs & SEG_A) hk_draw_rect(buf, x + t,     y,             w - 2 * t, t, colour);
        if (segs & SEG_G) hk_draw_rect(buf, x + t,     y + half - t / 2, w - 2 * t, t, colour);
        if (segs & SEG_D) hk_draw_rect(buf, x + t,     y + h - t,     w - 2 * t, t, colour);
        if (segs & SEG_F) hk_draw_rect(buf, x,         y + t,         t, half - t,  colour);
        if (segs & SEG_B) hk_draw_rect(buf, x + w - t, y + t,         t, half - t,  colour);
        if (segs & SEG_E) hk_draw_rect(buf, x,         y + half,      t, half - t,  colour);
        if (segs & SEG_C) hk_draw_rect(buf, x + w - t, y + half,      t, half - t,  colour);
    }
    return w + t * 2;   /* advance includes the gap to the next glyph */
}

int hk_draw_text(uint16_t *buf, int x, int y, int w, int h, int t,
                 const char *text, uint16_t colour)
{
    if (text == NULL) {
        return 0;
    }
    int advance = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        advance += hk_draw_glyph(buf, x + advance, y, w, h, t, text[i], colour);
    }
    return advance;
}

int hk_draw_text_width(int w, int thickness, int count)
{
    if (count <= 0 || w <= 0 || thickness <= 0) {
        return 0;
    }
    /* Exactly what hk_draw_glyph advances by, per glyph. */
    return count * (w + thickness * 2);
}
