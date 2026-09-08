#include "hk_gfx.h"

#include <stddef.h>

/*
 * Two rules hold this file together, and everything else is arithmetic.
 *
 * The first is hk_draw.c's: one function writes to the buffer and it clips.
 * The reason has not changed -- a stray write into 115 200 bytes of PSRAM shows
 * up as a corrupted heap somewhere else, seconds later -- but the stakes are
 * higher here, because these primitives read the pixel back before writing it.
 * A read past the end is as wrong as a write past the end and does not even
 * leave a mark. So hk_gfx_px() is still the only writer, and the two operations
 * that must read first (the vignette and the dim) never name a pixel of their
 * own: both scan the clip rectangle itself, which is the one range in this file
 * already known to lie inside the framebuffer. The vignette tests it again with
 * hk_gfx_px()'s own predicate, which costs nothing against the square root it
 * is already paying for and means a later change to those loop bounds cannot
 * quietly turn the read loose.
 *
 * The second is that antialiasing is a coverage number and nothing else. Every
 * shape here answers one question per pixel -- how much of this pixel is inside
 * the shape, 0 to 255 -- and hands the answer to the same compositor. Corners,
 * curves and the ends of an arc are all the same code path, which is why they
 * meet without a seam.
 */

#define FB_W HK_DRAW_WIDTH
#define FB_H HK_DRAW_HEIGHT

/* Geometry arrives in 1/16 px. A pixel's centre is half a pixel past its
 * index, and an edge fades across the one pixel it crosses, so 8 and 16 are
 * the only two subpixel constants that appear below. */
#define Q4_ONE  16
#define Q4_HALF 8

/* Bounds that exist so the arithmetic cannot leave int32, not because a caller
 * should ever approach them. A radius of 512 px already covers a 240 px panel
 * from any centre on it, and a centre 65 536 px away is off screen from every
 * direction; clamping both keeps dx*dx + dy*dy comfortably inside a signed
 * 32-bit int for every pixel the scan can reach. */
#define R_MAX_Q4 8192
#define C_MAX_Q4 (1 << 20)
#define C_MAX_PX 1024
#define R_MAX_PX 4096

/* --- clip ------------------------------------------------------------ */

/* Half-open, and never wider than the framebuffer. Module state rather than a
 * parameter because the caller that needs it -- a marquee, a scrolling list --
 * needs it to hold across a group of draws. */
static int s_x0 = 0;
static int s_y0 = 0;
static int s_x1 = FB_W;
static int s_y1 = FB_H;

static inline int clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void hk_gfx_clip_set(int x, int y, int w, int h)
{
    /* Computed wide, then clamped. A caller asking for INT_MAX width is asking
     * for the whole screen, not for x + w to wrap round to a negative edge. */
    const long long x1 = (long long)x + (long long)w;
    const long long y1 = (long long)y + (long long)h;

    s_x0 = clampi(x, 0, FB_W);
    s_y0 = clampi(y, 0, FB_H);
    s_x1 = (x1 < 0) ? 0 : ((x1 > FB_W) ? FB_W : (int)x1);
    s_y1 = (y1 < 0) ? 0 : ((y1 > FB_H) ? FB_H : (int)y1);

    /* An inside-out or empty rectangle clips everything away. Drawing nothing
     * is the honest reading of "clip to a box with no area"; the alternative is
     * a loop that never terminates the way its author expected. */
    if (s_x1 < s_x0) {
        s_x1 = s_x0;
    }
    if (s_y1 < s_y0) {
        s_y1 = s_y0;
    }
}

void hk_gfx_clip_reset(void)
{
    s_x0 = 0;
    s_y0 = 0;
    s_x1 = FB_W;
    s_y1 = FB_H;
}

static inline bool clip_has(int x, int y)
{
    return x >= s_x0 && y >= s_y0 && x < s_x1 && y < s_y1;
}

/* --- colour ---------------------------------------------------------- */

/*
 * The expansion replicates each channel's high bits into the low ones, so a
 * colour expanded and repacked is the colour it started as. That property is
 * the whole reason this is not a plain shift: without it, blending a pixel
 * against its own value would walk it one step darker every time, and a static
 * screen redrawn at 24 fps would visibly fade.
 */
static inline void unpack565(uint16_t c, int *r, int *g, int *b)
{
    const int r5 = (c >> 11) & 0x1F;
    const int g6 = (c >> 5) & 0x3F;
    const int b5 = c & 0x1F;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

static inline uint16_t pack565(int r, int g, int b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* a + (b - a) * t / 255, rounded away from zero so t = 0 lands exactly on a
 * and t = 255 exactly on b. The rounding is the point: an off-by-one at either
 * end is what turns "composite the same colour twice" into a slow drift. */
static inline int mix_ch(int a, int b, int t)
{
    const int d = (b - a) * t;
    return a + ((d >= 0) ? ((d + 127) / 255) : -(((-d) + 127) / 255));
}

/* Toward black, by the same rounding, so level = 255 is the identity. */
static inline int scale_ch(int v, int level)
{
    return (v * level + 127) / 255;
}

uint16_t hk_gfx_mix(uint16_t a, uint16_t b, uint8_t t)
{
    int ar, ag, ab, br, bg, bb;

    if (t == 0) {
        return a;
    }
    if (t == 255) {
        return b;
    }
    unpack565(a, &ar, &ag, &ab);
    unpack565(b, &br, &bg, &bb);
    return pack565(mix_ch(ar, br, t), mix_ch(ag, bg, t), mix_ch(ab, bb, t));
}

uint16_t hk_gfx_blend(uint16_t dst, uint16_t src, uint8_t alpha)
{
    /* Source-over onto an opaque destination is exactly a mix. Saying it once
     * means the two can never disagree about a rounding step. */
    return hk_gfx_mix(dst, src, alpha);
}

/* --- the one writer -------------------------------------------------- */

void hk_gfx_px(uint16_t *buf, int x, int y, uint16_t colour, uint8_t alpha)
{
    if (buf == NULL || alpha == 0) {
        return;
    }
    if (!clip_has(x, y)) {
        return;
    }
    {
        const size_t i = (size_t)y * FB_W + (size_t)x;
        buf[i] = (alpha == 255) ? colour : hk_gfx_blend(buf[i], colour, alpha);
    }
}

/* Coverage and the caller's alpha are independent: the shape says how much of
 * the pixel it touches, the caller says how solid it is. */
static inline uint8_t alpha_of(int coverage, uint8_t alpha)
{
    if (coverage <= 0) {
        return 0;
    }
    if (coverage >= 255) {
        return alpha;
    }
    if (alpha == 255) {
        return (uint8_t)coverage;
    }
    return (uint8_t)((coverage * (int)alpha + 127) / 255);
}

/* --- coverage from a signed distance --------------------------------- */

/* Inside by half a pixel is solid, outside by half a pixel is nothing, linear
 * between. The two early-outs run before the divide, so its numerator is always
 * positive and this ramp itself needs no opinion about how C rounds negatives.
 * The division that does is one level up, in cov_edge(). */
static inline int cov_from_sd_q4(int sd_q4)
{
    if (sd_q4 <= -Q4_HALF) {
        return 0;
    }
    if (sd_q4 >= Q4_HALF) {
        return 255;
    }
    return ((sd_q4 + Q4_HALF) * 255) / Q4_ONE;
}

/* The same ramp for a distance carried in Q15, which is the form the arc's
 * edge test produces. Shifting a negative right is implementation-defined, so
 * the early-outs make the value non-negative before the shift rather than
 * trusting the compiler to arithmetic-shift. */
#define SD15_HALF (Q4_HALF * 32768)
static inline int cov_from_sd_q15(int sd_q15)
{
    if (sd_q15 <= -SD15_HALF) {
        return 0;
    }
    if (sd_q15 >= SD15_HALF) {
        return 255;
    }
    return (int)((((uint32_t)(sd_q15 + SD15_HALF)) * 255u) >> 19);
}

/*
 * How far inside a circle of radius r a point is, from the squared distance,
 * without a square root: r*r - d*d factors as (r - d)(r + d), and anywhere near
 * the edge r + d is 2r. Only pixels already known to be within half a pixel of
 * the edge ever ask, so "near the edge" is not an approximation there, it is
 * the precondition. That is what keeps the divide count proportional to the
 * circumference instead of the area.
 *
 * This is the signed division: outside the radius the numerator is negative,
 * which is most of an antialiased edge, and C truncates toward zero instead of
 * flooring. That widens the one coverage step either side of the radius and
 * biases the edge outward by half of the 1/16 px this fixed point can express
 * -- below what the format resolves. Flooring instead was measured against a
 * supersampled reference and was no better, so the plain divide stays.
 */
static inline int cov_edge(int r_q4, int d2)
{
    return cov_from_sd_q4((r_q4 * r_q4 - d2) / (2 * r_q4));
}

static inline int floor_div16(int v)
{
    return (v >= 0) ? (v >> 4) : -(((-v) + 15) >> 4);
}

/* Binary-restoring integer square root. Used only where a real distance is
 * needed across a wide band -- the vignette's falloff and the iris's feather --
 * where the factorisation above would drift; never in a shape's edge test. */
static int isqrt32(uint32_t n)
{
    uint32_t rest = n;
    uint32_t root = 0;
    uint32_t bit = 1uL << 30;

    while (bit > rest) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (rest >= root + bit) {
            rest -= root + bit;
            root = root + 2 * bit;
        }
        root >>= 1;
        bit >>= 2;
    }
    return (int)root;
}

/* --- angles ---------------------------------------------------------- */

/*
 * sin(2*pi*a/1000) in Q15 for a quarter turn. One entry per per-mille step, so
 * an arc's angles need no interpolation, and the other three quadrants are
 * reflections of this one.
 *
 * hk_draw.c gets away without this: it only needs the ORDER of angles, so its
 * octant approximation is enough. This layer needs the direction itself,
 * because an arc's two ends are antialiased as straight lines through the
 * centre and the coverage is the perpendicular distance to them -- two
 * multiplies per pixel, exact, and no trigonometry in the loop.
 */
static const int16_t k_sin_q15[251] = {
         0,    206,    412,    618,    823,   1029,   1235,   1441,   1646,   1852,
      2058,   2263,   2468,   2674,   2879,   3084,   3289,   3493,   3698,   3903,
      4107,   4311,   4515,   4719,   4923,   5126,   5329,   5532,   5735,   5938,
      6140,   6342,   6544,   6746,   6947,   7148,   7349,   7549,   7750,   7949,
      8149,   8348,   8547,   8746,   8944,   9142,   9340,   9537,   9733,   9930,
     10126,  10321,  10517,  10711,  10906,  11100,  11293,  11486,  11679,  11871,
     12063,  12254,  12445,  12635,  12825,  13014,  13202,  13391,  13578,  13765,
     13952,  14138,  14323,  14508,  14693,  14876,  15060,  15242,  15424,  15605,
     15786,  15966,  16146,  16325,  16503,  16680,  16857,  17033,  17209,  17384,
     17558,  17731,  17904,  18076,  18248,  18418,  18588,  18757,  18926,  19094,
     19261,  19427,  19592,  19757,  19921,  20084,  20246,  20408,  20568,  20728,
     20887,  21045,  21203,  21359,  21515,  21670,  21824,  21977,  22129,  22281,
     22431,  22581,  22730,  22877,  23024,  23170,  23316,  23460,  23603,  23745,
     23887,  24027,  24167,  24305,  24443,  24580,  24715,  24850,  24984,  25116,
     25248,  25379,  25509,  25637,  25765,  25892,  26017,  26142,  26266,  26388,
     26510,  26630,  26750,  26868,  26986,  27102,  27217,  27331,  27444,  27556,
     27667,  27777,  27885,  27993,  28099,  28205,  28309,  28412,  28514,  28615,
     28715,  28813,  28911,  29007,  29102,  29197,  29289,  29381,  29472,  29561,
     29649,  29736,  29822,  29907,  29991,  30073,  30154,  30234,  30313,  30391,
     30467,  30542,  30616,  30689,  30760,  30831,  30900,  30968,  31035,  31100,
     31164,  31227,  31289,  31350,  31409,  31467,  31524,  31579,  31634,  31687,
     31739,  31789,  31838,  31886,  31933,  31979,  32023,  32066,  32108,  32148,
     32188,  32226,  32262,  32298,  32332,  32365,  32396,  32426,  32455,  32483,
     32510,  32535,  32559,  32581,  32603,  32623,  32641,  32659,  32675,  32690,
     32703,  32716,  32727,  32736,  32745,  32752,  32758,  32762,  32765,  32767,
     32767,
};

static int sin_pm(int a)
{
    a %= 1000;
    if (a < 0) {
        a += 1000;
    }
    if (a <= 250) {
        return k_sin_q15[a];
    }
    if (a <= 500) {
        return k_sin_q15[500 - a];
    }
    if (a <= 750) {
        return -k_sin_q15[a - 500];
    }
    return -k_sin_q15[1000 - a];
}

static inline int cos_pm(int a)
{
    return sin_pm(a + 250);
}

/* --- disc, ring, arc ------------------------------------------------- */

/*
 * What one scanline pass needs to know. Discs, rings and arcs share it because
 * they share an outer edge: a ring drawn as a disc minus a disc has a seam
 * wherever the two antialiased edges round differently, and the only way to be
 * sure that never happens is to compute both coverages for the same pixel in
 * the same pass and take the smaller.
 */
typedef struct {
    int outer_q4;
    int inner_q4;   /* <= 0 for a solid disc */
    int o_lo2;      /* squared radii bracketing the outer edge's fade band */
    int o_hi2;
    int i_lo2;      /* and the inner edge's */
    int i_hi2;
    int wedge;      /* 0 no angular limit, 1 intersect the two half-planes,
                     * -1 union them, which is what a sweep past half a turn is */
    int n0x, n0y;   /* inward normal of the start ray, Q15 */
    int n1x, n1y;   /* and of the end ray */
} annulus_t;

static void band_squares(int r_q4, int *lo2, int *hi2)
{
    const int lo = (r_q4 > Q4_HALF) ? (r_q4 - Q4_HALF) : 0;
    const int hi = r_q4 + Q4_HALF;
    *lo2 = lo * lo;
    *hi2 = hi * hi;
}

static void scan_annulus(uint16_t *buf, int cx_q4, int cy_q4,
                         const annulus_t *s, uint16_t colour, uint8_t alpha)
{
    /* One pixel of margin either side: the fade band reaches half a pixel past
     * the radius, so the last row that has anything in it is the one after the
     * last row the radius alone would name. */
    const int x0 = clampi(floor_div16(cx_q4 - s->outer_q4) - 1, s_x0, s_x1);
    const int x1 = clampi(floor_div16(cx_q4 + s->outer_q4) + 2, s_x0, s_x1);
    const int y0 = clampi(floor_div16(cy_q4 - s->outer_q4) - 1, s_y0, s_y1);
    const int y1 = clampi(floor_div16(cy_q4 + s->outer_q4) + 2, s_y0, s_y1);

    for (int y = y0; y < y1; y++) {
        const int dy = (y * Q4_ONE + Q4_HALF) - cy_q4;
        const int dy2 = dy * dy;
        int dx = (x0 * Q4_ONE + Q4_HALF) - cx_q4;

        for (int x = x0; x < x1; x++, dx += Q4_ONE) {
            const int d2 = dx * dx + dy2;
            int cov;

            if (d2 >= s->o_hi2) {
                continue;
            }
            cov = (d2 <= s->o_lo2) ? 255 : cov_edge(s->outer_q4, d2);

            if (s->inner_q4 > 0) {
                if (d2 <= s->i_lo2) {
                    continue;   /* through the hole */
                }
                if (d2 < s->i_hi2) {
                    const int out = 255 - cov_edge(s->inner_q4, d2);
                    if (out < cov) {
                        cov = out;
                    }
                }
            }

            if (s->wedge != 0) {
                const int c0 = cov_from_sd_q15(dx * s->n0x + dy * s->n0y);
                const int c1 = cov_from_sd_q15(dx * s->n1x + dy * s->n1y);
                const int ang = (s->wedge > 0) ? ((c0 < c1) ? c0 : c1)
                                               : ((c0 > c1) ? c0 : c1);
                if (ang < cov) {
                    cov = ang;
                }
            }

            hk_gfx_px(buf, x, y, colour, alpha_of(cov, alpha));
        }
    }
}

void hk_gfx_disc(uint16_t *buf, int cx_q4, int cy_q4, int r_q4,
                 uint16_t colour, uint8_t alpha)
{
    annulus_t s;

    if (buf == NULL || alpha == 0 || r_q4 <= 0) {
        return;
    }
    s.outer_q4 = clampi(r_q4, 1, R_MAX_Q4);
    s.inner_q4 = 0;
    s.i_lo2 = 0;
    s.i_hi2 = 0;
    s.wedge = 0;
    s.n0x = s.n0y = s.n1x = s.n1y = 0;
    band_squares(s.outer_q4, &s.o_lo2, &s.o_hi2);
    scan_annulus(buf, clampi(cx_q4, -C_MAX_Q4, C_MAX_Q4),
                 clampi(cy_q4, -C_MAX_Q4, C_MAX_Q4), &s, colour, alpha);
}

void hk_gfx_ring(uint16_t *buf, int cx_q4, int cy_q4, int outer_q4, int inner_q4,
                 uint16_t colour, uint8_t alpha)
{
    annulus_t s;

    /* An inner radius at or beyond the outer one is an empty ring, not a filled
     * plane -- the same trap hk_draw_ring() guards, and the same answer. */
    if (buf == NULL || alpha == 0 || outer_q4 <= 0 || inner_q4 >= outer_q4) {
        return;
    }
    s.outer_q4 = clampi(outer_q4, 1, R_MAX_Q4);
    s.inner_q4 = (inner_q4 > 0) ? clampi(inner_q4, 0, s.outer_q4 - 1) : 0;
    s.wedge = 0;
    s.n0x = s.n0y = s.n1x = s.n1y = 0;
    band_squares(s.outer_q4, &s.o_lo2, &s.o_hi2);
    band_squares(s.inner_q4, &s.i_lo2, &s.i_hi2);
    scan_annulus(buf, clampi(cx_q4, -C_MAX_Q4, C_MAX_Q4),
                 clampi(cy_q4, -C_MAX_Q4, C_MAX_Q4), &s, colour, alpha);
}

void hk_gfx_arc(uint16_t *buf, int cx_q4, int cy_q4, int outer_q4, int inner_q4,
                int start_per_mille, int sweep_per_mille,
                uint16_t colour, uint8_t alpha)
{
    annulus_t s;
    int a0, a1;

    if (buf == NULL || alpha == 0 || outer_q4 <= 0 || inner_q4 >= outer_q4 ||
        sweep_per_mille <= 0) {
        return;
    }
    if (sweep_per_mille >= 1000) {
        /* A full turn has no ends to antialias, and the wedge test at a sweep
         * of exactly 1000 would ask which side of a ray the ray itself is on. */
        hk_gfx_ring(buf, cx_q4, cy_q4, outer_q4, inner_q4, colour, alpha);
        return;
    }

    a0 = start_per_mille % 1000;
    if (a0 < 0) {
        a0 += 1000;
    }
    a1 = (a0 + sweep_per_mille) % 1000;   /* wrapping past twelve is ordinary */

    s.outer_q4 = clampi(outer_q4, 1, R_MAX_Q4);
    s.inner_q4 = (inner_q4 > 0) ? clampi(inner_q4, 0, s.outer_q4 - 1) : 0;
    band_squares(s.outer_q4, &s.o_lo2, &s.o_hi2);
    band_squares(s.inner_q4, &s.i_lo2, &s.i_hi2);

    /*
     * Angles are per-mille clockwise from twelve o'clock, matching
     * hk_draw_arc(): zero points up the screen and 250 points right. The
     * direction of the ray at angle a is (sin, -cos) in screen coordinates,
     * whose y grows downward, so the normal pointing into the sweep is
     * (cos, sin) at the start and its negation at the end.
     *
     * Below half a turn the wedge is where both half-planes agree; above it,
     * the wedge is everything except where they both disagree, so the test
     * becomes a union. Getting this backwards leaves a progress ring stuck at
     * half and then inverted.
     */
    s.n0x = cos_pm(a0);
    s.n0y = sin_pm(a0);
    s.n1x = -cos_pm(a1);
    s.n1y = -sin_pm(a1);
    s.wedge = (sweep_per_mille <= 500) ? 1 : -1;

    scan_annulus(buf, clampi(cx_q4, -C_MAX_Q4, C_MAX_Q4),
                 clampi(cy_q4, -C_MAX_Q4, C_MAX_Q4), &s, colour, alpha);
}

/* --- rectangles ------------------------------------------------------ */

void hk_gfx_rect(uint16_t *buf, int x, int y, int w, int h,
                 uint16_t colour, uint8_t alpha)
{
    long long ex, ey;
    int x1, y1;

    if (buf == NULL || alpha == 0 || w <= 0 || h <= 0) {
        return;
    }
    /* Widened before it is clamped, for the same reason hk_gfx_clip_set() does
     * it: a caller asking for a rectangle wider than the world wants the world,
     * not an edge that wrapped round to a negative number. */
    ex = (long long)x + (long long)w;
    ey = (long long)y + (long long)h;
    x1 = (ex > FB_W) ? FB_W : ((ex < 0) ? 0 : (int)ex);
    y1 = (ey > FB_H) ? FB_H : ((ey < 0) ? 0 : (int)ey);
    x1 = clampi(x1, s_x0, s_x1);
    y1 = clampi(y1, s_y0, s_y1);
    x = clampi(x, s_x0, s_x1);
    y = clampi(y, s_y0, s_y1);

    for (int row = y; row < y1; row++) {
        for (int col = x; col < x1; col++) {
            hk_gfx_px(buf, col, row, colour, alpha);
        }
    }
}

/*
 * A rounded rectangle is every point within `radius` of the rectangle its
 * corner circles roll around. Written that way, one expression covers the four
 * corners and the four straight edges, and there is no join between them to get
 * wrong -- the straight run reaches exactly full coverage at the same distance
 * the corner does, because it is the same distance.
 */
static int rrect_cov(int px_q4, int py_q4,
                     int ix0, int iy0, int ix1, int iy1,
                     int r_q4, int lo2, int hi2)
{
    int dx = 0;
    int dy = 0;
    int d2;

    if (px_q4 < ix0) {
        dx = ix0 - px_q4;
    } else if (px_q4 > ix1) {
        dx = px_q4 - ix1;
    }
    if (py_q4 < iy0) {
        dy = iy0 - py_q4;
    } else if (py_q4 > iy1) {
        dy = py_q4 - iy1;
    }
    if (dx == 0 && dy == 0) {
        return 255;
    }
    if (r_q4 <= 0) {
        return 0;   /* a square corner: the pixel centre is in or it is out */
    }
    d2 = dx * dx + dy * dy;
    if (d2 <= lo2) {
        return 255;
    }
    if (d2 >= hi2) {
        return 0;
    }
    return cov_edge(r_q4, d2);
}

/* The rounded rectangle's parameters, resolved once so the two entry points
 * cannot clamp the radius differently. */
typedef struct {
    int x0, y0, x1, y1;     /* pixel bounds, half-open */
    int ix0, iy0, ix1, iy1; /* the inner rectangle, 1/16 px */
    int r_q4;
    int lo2, hi2;
} rrect_t;

/* Room for a rectangle that starts well off screen and ends well past it, with
 * enough headroom left that the squared corner distance stays inside an int. */
#define RR_MAX_XY 8192
#define RR_MAX_WH 16384
#define RR_MAX_R  2048

static bool rrect_setup(rrect_t *o, int x, int y, int w, int h, int radius)
{
    int r;

    if (w <= 0 || h <= 0) {
        return false;
    }
    x = clampi(x, -RR_MAX_XY, RR_MAX_XY);
    y = clampi(y, -RR_MAX_XY, RR_MAX_XY);
    w = clampi(w, 1, RR_MAX_WH);
    h = clampi(h, 1, RR_MAX_WH);

    r = clampi(radius, 0, RR_MAX_R);
    if (r > w / 2) {
        r = w / 2;
    }
    if (r > h / 2) {
        r = h / 2;
    }

    o->x0 = x;
    o->y0 = y;
    o->x1 = x + w;
    o->y1 = y + h;
    o->ix0 = (x + r) * Q4_ONE;
    o->iy0 = (y + r) * Q4_ONE;
    o->ix1 = (x + w - r) * Q4_ONE;
    o->iy1 = (y + h - r) * Q4_ONE;
    o->r_q4 = r * Q4_ONE;
    band_squares(o->r_q4, &o->lo2, &o->hi2);
    return true;
}

static inline int rrect_cov_at(const rrect_t *r, int px_q4, int py_q4)
{
    return rrect_cov(px_q4, py_q4, r->ix0, r->iy0, r->ix1, r->iy1,
                     r->r_q4, r->lo2, r->hi2);
}

void hk_gfx_rrect(uint16_t *buf, int x, int y, int w, int h, int radius,
                  uint16_t colour, uint8_t alpha)
{
    rrect_t r;
    int cx0, cy0, cx1, cy1;

    if (buf == NULL || alpha == 0 || !rrect_setup(&r, x, y, w, h, radius)) {
        return;
    }
    cx0 = clampi(r.x0, s_x0, s_x1);
    cy0 = clampi(r.y0, s_y0, s_y1);
    cx1 = clampi(r.x1, s_x0, s_x1);
    cy1 = clampi(r.y1, s_y0, s_y1);

    for (int py = cy0; py < cy1; py++) {
        const int py_q4 = py * Q4_ONE + Q4_HALF;
        for (int px = cx0; px < cx1; px++) {
            const int cov = rrect_cov_at(&r, px * Q4_ONE + Q4_HALF, py_q4);
            hk_gfx_px(buf, px, py, colour, alpha_of(cov, alpha));
        }
    }
}

void hk_gfx_rrect_outline(uint16_t *buf, int x, int y, int w, int h, int radius,
                          int stroke, uint16_t colour, uint8_t alpha)
{
    rrect_t out;
    rrect_t in;
    bool hollow;
    int cx0, cy0, cx1, cy1;

    if (buf == NULL || alpha == 0 || stroke <= 0) {
        return;
    }
    /* Clamped here as well as inside rrect_setup(), because the inset below
     * does arithmetic on these before they get there. */
    stroke = clampi(stroke, 1, RR_MAX_WH);
    radius = clampi(radius, 0, RR_MAX_R);
    x = clampi(x, -RR_MAX_XY, RR_MAX_XY);
    y = clampi(y, -RR_MAX_XY, RR_MAX_XY);
    w = clampi(w, 1, RR_MAX_WH);
    h = clampi(h, 1, RR_MAX_WH);
    if (!rrect_setup(&out, x, y, w, h, radius)) {
        return;
    }
    /* A stroke that meets itself in the middle is a filled shape, and saying so
     * here is better than letting the inner rectangle come out inside-out. */
    hollow = rrect_setup(&in, x + stroke, y + stroke,
                         w - 2 * stroke, h - 2 * stroke, radius - stroke);

    cx0 = clampi(out.x0, s_x0, s_x1);
    cy0 = clampi(out.y0, s_y0, s_y1);
    cx1 = clampi(out.x1, s_x0, s_x1);
    cy1 = clampi(out.y1, s_y0, s_y1);

    for (int py = cy0; py < cy1; py++) {
        const int py_q4 = py * Q4_ONE + Q4_HALF;
        for (int px = cx0; px < cx1; px++) {
            const int px_q4 = px * Q4_ONE + Q4_HALF;
            int cov = rrect_cov_at(&out, px_q4, py_q4);

            if (cov == 0) {
                continue;
            }
            if (hollow) {
                const int keep = 255 - rrect_cov_at(&in, px_q4, py_q4);
                if (keep < cov) {
                    cov = keep;
                }
            }
            hk_gfx_px(buf, px, py, colour, alpha_of(cov, alpha));
        }
    }
}

/* --- coverage masks -------------------------------------------------- */

void hk_gfx_blit_a8(uint16_t *buf, int x, int y, int w, int h,
                    const uint8_t *coverage, int stride,
                    uint16_t colour, uint8_t alpha)
{
    if (buf == NULL || coverage == NULL || alpha == 0 || w <= 0 || h <= 0) {
        return;
    }
    /* The whole glyph off the edge of its clip is the common case for a
     * marquee, and it is worth one test rather than w*h of them. */
    if (x >= s_x1 || y >= s_y1 ||
        (long long)x + w <= s_x0 || (long long)y + h <= s_y0) {
        return;
    }
    for (int row = 0; row < h; row++) {
        const uint8_t *src = coverage + (size_t)row * (size_t)stride;
        const int py = y + row;

        if (py < s_y0 || py >= s_y1) {
            continue;
        }
        for (int col = 0; col < w; col++) {
            hk_gfx_px(buf, x + col, py, colour, alpha_of(src[col], alpha));
        }
    }
}

void hk_gfx_blit_a4(uint16_t *buf, int x, int y, int w, int h,
                    const uint8_t *coverage, int stride_bytes,
                    uint16_t colour, uint8_t alpha)
{
    if (buf == NULL || coverage == NULL || alpha == 0 || w <= 0 || h <= 0) {
        return;
    }
    if (x >= s_x1 || y >= s_y1 ||
        (long long)x + w <= s_x0 || (long long)y + h <= s_y0) {
        return;
    }
    for (int row = 0; row < h; row++) {
        const uint8_t *src = coverage + (size_t)row * (size_t)stride_bytes;
        const int py = y + row;

        if (py < s_y0 || py >= s_y1) {
            continue;
        }
        for (int col = 0; col < w; col++) {
            /* High nibble first, and the row's padding to a whole byte is
             * already in stride_bytes -- recomputing it here is how a font with
             * an odd width ends up sheared one pixel further every row. */
            const uint8_t byte = src[col >> 1];
            const int nib = ((col & 1) == 0) ? (byte >> 4) : (byte & 0x0F);

            hk_gfx_px(buf, x + col, py, colour, alpha_of(nib * 17, alpha));
        }
    }
}

/* --- vignette, dim, iris --------------------------------------------- */

void hk_gfx_vignette(uint16_t *buf, int cx, int cy, int inner_r, int outer_r,
                     uint8_t strength)
{
    int i2, o2, span, k;

    if (buf == NULL || strength == 0) {
        return;
    }
    cx = clampi(cx, -C_MAX_PX, C_MAX_PX);
    cy = clampi(cy, -C_MAX_PX, C_MAX_PX);
    inner_r = clampi(inner_r, 0, R_MAX_PX);
    outer_r = clampi(outer_r, 0, R_MAX_PX);
    if (outer_r <= inner_r) {
        outer_r = inner_r + 1;   /* a hard edge rather than a division by zero */
    }
    i2 = inner_r * inner_r;
    o2 = outer_r * outer_r;
    span = outer_r - inner_r;
    k = (255 * 65536) / span;

    for (int y = s_y0; y < s_y1; y++) {
        const int dy = y - cy;
        const int dy2 = dy * dy;

        for (int x = s_x0; x < s_x1; x++) {
            const int dx = x - cx;
            const int d2 = dx * dx + dy2;
            int t, smooth, level, r, g, b;

            if (d2 <= i2) {
                continue;   /* the image proper, untouched */
            }
            if (d2 >= o2) {
                t = 255;
            } else {
                /* A real distance here, not the near-the-edge factorisation:
                 * the band is tens of pixels wide and the approximation drifts
                 * enough across it to show as a ring. */
                t = ((isqrt32((uint32_t)d2) - inner_r) * k) >> 16;
                t = clampi(t, 0, 255);
            }
            /* Smoothstep, because a linear ramp leaves a visible crease where
             * it starts and where it stops -- the eye finds the discontinuity
             * in the slope, not in the value. */
            smooth = (t * t * (765 - 2 * t)) / 65025;
            level = 255 - ((int)strength * smooth + 127) / 255;

            if (!clip_has(x, y)) {
                continue;
            }
            unpack565(buf[(size_t)y * FB_W + (size_t)x], &r, &g, &b);
            hk_gfx_px(buf, x, y,
                      pack565(scale_ch(r, level), scale_ch(g, level),
                              scale_ch(b, level)), 255);
        }
    }
}

void hk_gfx_dim(uint16_t *buf, uint8_t level)
{
    uint16_t tr[32];
    uint16_t tg[64];
    uint16_t tb[32];

    if (buf == NULL || level == 255) {
        return;
    }
    /* Three small tables instead of arithmetic per pixel. The whole screen goes
     * through here on every frame of a transition, and 128 entries built once
     * is cheaper than 57 600 pixels scaled three channels at a time. */
    for (int i = 0; i < 32; i++) {
        const int v = scale_ch((i << 3) | (i >> 2), level);
        tr[i] = (uint16_t)((v & 0xF8) << 8);
        tb[i] = (uint16_t)(v >> 3);
    }
    for (int i = 0; i < 64; i++) {
        const int v = scale_ch((i << 2) | (i >> 4), level);
        tg[i] = (uint16_t)((v & 0xFC) << 3);
    }

    for (int y = s_y0; y < s_y1; y++) {
        for (int x = s_x0; x < s_x1; x++) {
            const uint16_t c = buf[(size_t)y * FB_W + (size_t)x];
            hk_gfx_px(buf, x, y,
                      (uint16_t)(tr[c >> 11] | tg[(c >> 5) & 0x3F] | tb[c & 0x1F]),
                      255);
        }
    }
}

void hk_gfx_iris(uint16_t *buf, int cx, int cy, int radius_q4, int feather_q4,
                 uint16_t outside, bool keep_inside)
{
    int r_q4, f_q4, half, lo, hi, lo2, hi2, cx_q4, cy_q4;

    if (buf == NULL) {
        return;
    }
    cx_q4 = clampi(cx, -C_MAX_PX, C_MAX_PX) * Q4_ONE;
    cy_q4 = clampi(cy, -C_MAX_PX, C_MAX_PX) * Q4_ONE;
    r_q4 = clampi(radius_q4, 0, R_MAX_Q4);
    /* No feather still gets one pixel of it: a hard circular edge on a 240 px
     * panel crawls visibly as the radius animates. */
    f_q4 = clampi(feather_q4, Q4_ONE, R_MAX_Q4);
    half = f_q4 / 2;
    lo = (r_q4 > half) ? (r_q4 - half) : 0;
    hi = r_q4 + half;
    lo2 = lo * lo;
    hi2 = hi * hi;

    for (int y = s_y0; y < s_y1; y++) {
        const int dy = (y * Q4_ONE + Q4_HALF) - cy_q4;
        const int dy2 = dy * dy;

        for (int x = s_x0; x < s_x1; x++) {
            const int dx = (x * Q4_ONE + Q4_HALF) - cx_q4;
            const int d2 = dx * dx + dy2;
            int inside;

            if (d2 <= lo2) {
                inside = 255;
            } else if (d2 >= hi2) {
                inside = 0;
            } else {
                inside = ((r_q4 + half - isqrt32((uint32_t)d2)) * 255) / f_q4;
                inside = clampi(inside, 0, 255);
            }

            /* The kept side is untouched and the other side is painted over.
             * Expressed as an alpha, so the feather is the same composite every
             * other primitive uses rather than a second blending rule. */
            {
                const int keep = keep_inside ? inside : (255 - inside);
                hk_gfx_px(buf, x, y, outside, (uint8_t)(255 - keep));
            }
        }
    }
}
