/**
 * @file hk_sky.c
 * @brief The galaxy: a table walk, not a simulation.
 *
 * Everything expensive happens once. hk_sky_init() builds the tables and
 * nothing after that allocates, calls libm, or asks a trigonometric question:
 *
 *   - a pixel-to-polar map, one quadrant of it, because the full 240x240 map
 *     costs 115 KB and the disc is symmetric about both axes anyway;
 *   - the nebula, stored in (radius, arm phase) instead of (radius, angle), so
 *     a rotation is an integer added to an index rather than a resampling --
 *     and so the arms stay arms instead of winding shut over an afternoon;
 *   - the star table, polar for the same reason;
 *   - two small per-frame tables, rebuilt at the top of each render, carrying
 *     the mood, the master level, the breath and the boot animation. Folding
 *     those four into a lookup is what keeps the inner loop to four loads and
 *     a store: nothing per-pixel knows what mood it is in.
 *
 * The inner loop reads the map, indexes the nebula, indexes the colour table
 * and writes one pixel. It never reads the framebuffer back -- that is the
 * expensive direction on PSRAM -- and never writes a pixel twice. Stars are
 * the deliberate exception: about 1600 of them touching some 7000 pixels,
 * which they have to blend into rather than overwrite.
 *
 * Face-on, as hk_sky.h says. That is not only a look: the background is
 * sampled through a circular polar map, so a tilted star field would sit at an
 * angle to its own arms and the two would visibly disagree.
 */
#include "hk_sky.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hk_draw.h"
#include "hk_palette.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#endif

#define SKY_W        HK_DRAW_WIDTH
#define SKY_H        HK_DRAW_HEIGHT
#define SKY_HALF     (SKY_W / 2)          /* the map covers one quadrant     */
#define SKY_C_Q4     ((SKY_HALF * 16) - 8) /* centre at 119.5 px, in 1/16 px */

#define SKY_PI       3.14159265358979f

/*
 * Radius is carried as an index, not as pixels, so one 256-entry table serves
 * the haze, the aura, the core, the nebula gain and the vignette at once. 255
 * is the corner of the square -- 169 px -- which is 0.663 px per step, finer
 * than RGB565 can show.
 */
#define SKY_R_STEPS  256
#define SKY_CORNER   169.0f
#define SKY_ROWS     260              /* 256 plus the dither's headroom      */

/* Angle is 1024 to the turn for the map, 65536 for the stars. */
#define SKY_ANG      1024
#define SKY_ANG_MASK (SKY_ANG - 1)

/* The nebula field. 64 radial rows is 2.7 px a row and 256 phase cells is
 * 2.5 px across at the arms; both are below what the ordered dither below
 * hides, and together they are 16 KB instead of the 115 KB a screen-space
 * field would have cost. */
#define NEB_R        64
#define NEB_A        256
#define NEB_A_MASK   (NEB_A - 1)

/* Nebula intensity reaches the colour table quantised to 17 steps. Any finer
 * is wasted: the panel has 32 levels of red to spend in total. */
#define MIX_LEVELS   17

/* The spiral every arm, dust lane and arm star is placed on: r = a*e^(b*th).
 * One pair of constants, used by the field and the star table both, because
 * two spirals that nearly agree read as a smear rather than as arms. */
#define ARM_A        5.5f
#define ARM_B        0.615f
/* th in radians -> arm phase in 1024ths of a turn. */
#define ARM_UNITS    (SKY_ANG / (2.0f * SKY_PI) / ARM_B)

#define SKY_STARS    1600
#define SKY_BEACONS  6

/* Populations. They differ in where they sit and, more importantly, in how
 * fast they come round: see rotation_rates(). */
#define POP_BULGE    0u
#define POP_ARM      1u
#define POP_HALO     2u

/* Colour slots a star can carry. Four, from the palette, so a star can never
 * be a colour nothing else on the screen is. */
#define COL_CORE     0u
#define COL_STAR     1u
#define COL_HII      2u
#define COL_ACCENT   3u
#define SKY_COLS     4

/*
 * The palette speaks RGB565; compositing needs 8-bit channels. Bit
 * replication expands them back to exactly the value the panel will show, so
 * this is a conversion rather than a second guess at the design.
 */
#define UNP_R(c) ((uint8_t)((((c) >> 11) & 31u) * 255u / 31u))
#define UNP_G(c) ((uint8_t)((((c) >> 5) & 63u) * 255u / 63u))
#define UNP_B(c) ((uint8_t)(((c) & 31u) * 255u / 31u))

typedef struct {
    uint16_t rad_q4;   /* distance from centre, 1/16 px                     */
    uint16_t ang;      /* 0..65535 is one turn, clockwise from twelve       */
    uint8_t  bright;   /* weight before twinkle, mood and level             */
    uint8_t  rate;     /* twinkle rate; see the modulus in render_stars()   */
    uint8_t  phase;    /* twinkle phase, so no two share a clock            */
    uint8_t  kind;     /* population in bits 0-1, colour 2-3, size in bit 4 */
} sky_star_t;

typedef struct {
    uint16_t rad_q4;
    uint16_t ang;
    uint8_t  col;
    uint8_t  halo;     /* halo radius in whole px                           */
    uint8_t  rate;
    uint8_t  phase;
} sky_beacon_t;

/*
 * A mood is a set of scalars, never a different geometry. Same stars, same
 * arms, same bar; brightness, saturation, colour temperature and rotation
 * rate are all that move. A screen that changed its background would read as
 * a different device rather than as the same one in a different state.
 */
typedef struct {
    uint16_t gain;     /* 256 = nominal brightness                          */
    uint16_t core;     /* the bulge glow, separately, so CALM can lose it   */
    uint16_t neb;      /* nebula strength                                   */
    uint16_t star;     /* star brightness                                   */
    uint16_t spin;     /* 256 = nominal rotation                            */
    uint16_t sat;      /* 256 = as designed, 0 = grey                       */
    int16_t  warm;     /* red added and blue removed, in 8-bit units        */
} sky_mood_cfg_t;

static const sky_mood_cfg_t s_moods[HK_SKY_MOOD_COUNT] = {
    /* BOOT   */ { 282, 300, 300, 300, 256, 256,   0 },
    /* GALAXY */ { 256, 256, 256, 256, 256, 256,   0 },
    /* CALM   */ { 118,  46, 112, 120, 112, 208,  -6 },
    /* EMBER  */ { 216, 250, 224, 224, 190, 256, 110 },
    /* ALERT  */ { 140,  96, 120, 168,  16,  56, -40 },
};

/* --- built once ----------------------------------------------------- */

static uint16_t   *s_map;      /* one quadrant: (r_idx << 8) | quad angle   */
static uint8_t    *s_neb;      /* NEB_R * NEB_A, intensity in arm phase     */
static sky_star_t *s_stars;
static sky_beacon_t s_beacon[SKY_BEACONS];
static bool        s_ready;

static int16_t  s_sin[SKY_ANG];        /* Q15                               */
static bool     s_trig;

static uint8_t  s_haze[SKY_R_STEPS];   /* the disc's outer glow             */
static uint8_t  s_aura[SKY_R_STEPS];   /* the breathing violet              */
static uint8_t  s_core[SKY_R_STEPS];   /* the bulge                         */
static uint8_t  s_nebg[SKY_R_STEPS];   /* how much nebula lives at r        */
static uint8_t  s_nebw[SKY_R_STEPS];   /* how warm the gas is at r          */
static uint8_t  s_vig[SKY_R_STEPS];    /* 255 inside the glass, 0 outside   */
static uint16_t s_lnterm[SKY_R_STEPS]; /* the spiral's phase offset at r    */
static uint8_t  s_halo[65];            /* beacon falloff by d^2             */

/* Boot is a function of t_ms alone -- no stored phase -- so these are the
 * curve sampled every 50 ms and interpolated between. */
#define BOOT_STEPS   33
#define BOOT_STEP_MS 50
static uint16_t s_boot_scale[BOOT_STEPS];  /* Q8, 256 = settled             */
static uint16_t s_boot_gain[BOOT_STEPS];   /* Q8, 256 = full               */
static uint16_t s_boot_turn[BOOT_STEPS];   /* extra rotation, 65536ths      */

/* --- rebuilt every frame -------------------------------------------- */

static uint16_t s_mix[SKY_ROWS * MIX_LEVELS];
static uint32_t s_aux[SKY_ROWS];       /* (nebula row << 16) | phase warp   */
static uint16_t s_line[SKY_W];         /* a row, assembled in internal RAM  */
static uint8_t  s_starcol[SKY_COLS][3];
static uint32_t s_last_us;

/* Ordered dither. The aura is a 100 px ramp and RGB565 gives it about
 * twenty steps of blue; without this it arrives as visible rings. The same
 * cell dithers the radius and the nebula level, which is why one 4x4 table
 * does both jobs. */
static const uint8_t s_bayer[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

/* ------------------------------------------------------------------ */
/* small maths, all of it init-time except the trig table             */
/* ------------------------------------------------------------------ */

/*
 * Our own generator, not rand(): the seed is a promise that every speaker
 * shows the same sky, and rand() keeps that promise only within one libc.
 * xorshift32 is enough -- the stars want to look unplanned, not to resist
 * analysis.
 */
static uint32_t s_rng = 1u;

static uint32_t rnd_u32(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}

static float frnd(void)
{
    return (float)(rnd_u32() >> 8) * (1.0f / 16777216.0f);
}

/* Two uniforms summed is a triangle: a rough normal, and the shape the bulge
 * and the across-arm scatter both want. */
static float rnd2(void)
{
    return frnd() + frnd() - 1.0f;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void build_trig(void)
{
    if (s_trig) {
        return;
    }
    for (int i = 0; i < SKY_ANG; i++) {
        const float a = (float)i * (2.0f * SKY_PI / (float)SKY_ANG);
        s_sin[i] = (int16_t)lrintf(sinf(a) * 32767.0f);
    }
    s_trig = true;
}

static float idx_px(int i)
{
    return (float)i * (SKY_CORNER / 255.0f);
}

/* Falloff shared by every soft blob here: 1 at the centre, 0 at the edge,
 * flat where it meets zero so the edge does not print a ring. */
static float falloff(float u)
{
    if (u >= 1.0f) {
        return 0.0f;
    }
    const float v = 1.0f - u * u;
    return v * v;
}

static uint8_t f2u8(float v)
{
    if (v <= 0.0f) {
        return 0u;
    }
    if (v >= 1.0f) {
        return 255u;
    }
    return (uint8_t)lrintf(v * 255.0f);
}

/* ------------------------------------------------------------------ */
/* the tables that are built once                                      */
/* ------------------------------------------------------------------ */

/*
 * The pixel-to-polar map, one quadrant.
 *
 * The centre sits at 119.5, so |dx| and |dy| are half-integers and the four
 * quadrants are exact mirrors of each other -- no seam down the middle, and
 * a quarter of the memory. Each entry is (radius index << 8) | quadrant
 * angle; one 16-bit load per pixel gives the renderer both.
 */
static bool build_map(void)
{
    for (int qy = 0; qy < SKY_HALF; qy++) {
        const float ady = (float)qy + 0.5f;
        for (int qx = 0; qx < SKY_HALF; qx++) {
            const float adx = (float)qx + 0.5f;
            const float r = sqrtf(adx * adx + ady * ady);
            const float a = atan2f(ady, adx) * ((float)SKY_ANG / (2.0f * SKY_PI));
            const int ri = clampi((int)lrintf(r * (255.0f / SKY_CORNER)), 0, 255);
            const int ai = clampi((int)lrintf(a), 0, 255);
            s_map[(size_t)qy * SKY_HALF + (size_t)qx] =
                (uint16_t)(((uint32_t)ri << 8) | (uint32_t)ai);
        }
    }
    return true;
}

/*
 * The radial profiles. Everything that depends only on how far out you are
 * lives here, so the per-frame table build is a handful of lookups rather
 * than a handful of exponentials.
 */
static void build_radial(void)
{
    for (int i = 0; i < SKY_R_STEPS; i++) {
        const float rp = idx_px(i);

        /* The disc's own haze. Wide and weak: it is the thing that stops the
         * arms from being suspended in nothing. */
        s_haze[i] = f2u8(expf(-(rp * rp) / (62.0f * 62.0f)));

        /* The aura and the core are sampled through a scale factor at frame
         * time, which is how they breathe without a second table. */
        s_aura[i] = f2u8(falloff(rp / 55.0f));
        s_core[i] = f2u8(0.62f * falloff(rp / 24.0f) + 0.38f * falloff(rp / 9.0f));

        /* Gas lives in the disc, not in the nucleus and not in the halo. */
        {
            float g = falloff((rp - 52.0f) / 74.0f);
            if (rp < 13.0f) {
                g *= rp / 13.0f;
            }
            s_nebg[i] = f2u8(g);
        }

        /* Gas near the bulge is lit by old warm stars, so it warms toward the
         * core colour instead of staying violet all the way in. */
        s_nebw[i] = f2u8(falloff(rp / 46.0f));

        /* The panel is round and its bezel is black. Without this the picture
         * is a rectangle behind a hole rather than something in the glass. */
        if (rp <= 111.0f) {
            s_vig[i] = 255u;
        } else if (rp >= 122.0f) {
            s_vig[i] = 0u;
        } else {
            const float u = (rp - 111.0f) / 11.0f;
            s_vig[i] = f2u8(1.0f - u * u * (3.0f - 2.0f * u));
        }

        /* Where the spiral's phase zero sits at this radius. Subtracting this
         * from a pixel's angle is what turns (r, angle) into (r, arm phase),
         * and it is the whole reason a rotation is an integer add. */
        {
            const float rr = rp < 1.0f ? 1.0f : rp;
            const int u = (int)lrintf(logf(rr / ARM_A) * ARM_UNITS);
            s_lnterm[i] = (uint16_t)((uint32_t)(u & SKY_ANG_MASK));
        }
    }

    for (int i = 0; i < 65; i++) {
        /* Indexed by squared distance, so this is (1 - d/R)^3: concentrated
         * in the middle, flat where it reaches zero. The falloff() above
         * stays near its peak for half the radius and prints as a disc with
         * an edge rather than as a glow. */
        const float u = sqrtf((float)i / 64.0f);
        s_halo[i] = f2u8((1.0f - u) * (1.0f - u) * (1.0f - u));
    }
}

/*
 * Boot, sampled.
 *
 * hk_sky_render() takes t_ms and nothing else, so the opening cannot keep a
 * phase between calls; a curve sampled every 50 ms and interpolated gives the
 * same answer for the same t whether it is the first frame or a repeat.
 *
 * The stars start far out and fall in, the light comes up behind them, and
 * the whole field arrives already turning and slows into its steady rate --
 * a universe caught mid-spin rather than one switched on.
 */
static void build_boot(void)
{
    float turn = 0.0f;
    for (int i = 0; i < BOOT_STEPS; i++) {
        const float t = (float)(i * BOOT_STEP_MS) / 1000.0f;
        const float p = t / 1.2f > 1.0f ? 1.0f : t / 1.2f;
        const float k = 1.0f - p;
        s_boot_scale[i] = (uint16_t)lrintf(256.0f * (1.0f + 1.6f * k * k * k));

        {
            const float g = t / 0.6f > 1.0f ? 1.0f : t / 0.6f;
            s_boot_gain[i] = (uint16_t)lrintf(256.0f * g * g * (3.0f - 2.0f * g));
        }

        /* The integral of a decaying boost, accumulated on the same grid it
         * is read back on, so the two cannot disagree. */
        s_boot_turn[i] = (uint16_t)((uint32_t)lrintf(turn * 65536.0f) & 0xFFFFu);
        turn += (7.5f * expf(-t * 0.85f)) * ((float)BOOT_STEP_MS / 1000.0f) / 60.0f;
    }
}

/* Value noise on a coarse grid, wrapped in phase. The arms need to be lumpy:
 * a spiral drawn from the formula alone reads as a diagram. */
#define NOISE_R 20
#define NOISE_A 48

/* The grid lives in the scratch buffer build_nebula() already needs, not in a
 * static of its own: it is read for the twenty milliseconds the field takes
 * to build and never again, and 3.8 KB of internal RAM is worth more to the
 * rest of the firmware than to that. */
static float noise_at(const float *s_noise_, float fr, float fa)
{
    const float (*s_noise)[NOISE_A] = (const float (*)[NOISE_A])s_noise_;
    const int r0 = clampi((int)fr, 0, NOISE_R - 2);
    const int a0 = ((int)fa) % NOISE_A;
    const float tr = fr - (float)r0;
    const float ta = fa - floorf(fa);
    const int a1 = (a0 + 1) % NOISE_A;
    const float n0 = s_noise[r0][a0] + (s_noise[r0][a1] - s_noise[r0][a0]) * ta;
    const float n1 = s_noise[r0 + 1][a0] + (s_noise[r0 + 1][a1] - s_noise[r0 + 1][a0]) * ta;
    return n0 + (n1 - n0) * tr;
}

/*
 * The nebula, in (radius, arm phase).
 *
 * Two arms sampled off the same logarithmic spiral the arm stars are, each
 * with a dust lane trailing just inside it -- the lane is drawn as an absence
 * of gas rather than as absorption, because the compositing here is additive
 * and a lane reads as dark from the contrast with the ridge beside it. A bar
 * crosses the middle, which is what makes this a barred spiral rather than a
 * pinwheel, and it is written in screen angle because a bar rotates rigidly
 * whatever the gas around it does.
 */
static void build_nebula(float *tmp, float *noise)
{
    int rr, p, i;

    for (i = 0; i < NOISE_R * NOISE_A; i++) {
        noise[i] = frnd();
    }

    for (rr = 0; rr < NEB_R; rr++) {
        const int ridx = rr * 4 + 2;
        const float rp = idx_px(ridx);
        const float env = (float)s_nebg[clampi(ridx, 0, 255)] / 255.0f;
        const float w = 5.5f + rp * 0.085f;
        const float ln = (float)s_lnterm[clampi(ridx, 0, 255)];

        for (p = 0; p < NEB_A; p++) {
            const float psi = (float)(p * 4 + 2);
            float v = 0.0f;

            /* the two arms, and the lane each one trails */
            for (i = 0; i < 2; i++) {
                float d = psi - (float)(i * (SKY_ANG / 2));
                while (d > (float)(SKY_ANG / 2)) { d -= (float)SKY_ANG; }
                while (d < -(float)(SKY_ANG / 2)) { d += (float)SKY_ANG; }
                {
                    const float across = d * rp * (2.0f * SKY_PI / (float)SKY_ANG);
                    const float u = across / w;
                    const float lane = (across + 4.2f) / (0.62f * w);
                    v += expf(-u * u);
                    v -= 0.80f * expf(-lane * lane);
                }
            }

            /* the bar, in screen angle: it turns with the pattern, not with
             * the gas, which is what a bar does */
            {
                float th = psi + ln;
                while (th >= (float)SKY_ANG) { th -= (float)SKY_ANG; }
                {
                    float dt = th;
                    while (dt > (float)(SKY_ANG / 4)) { dt -= (float)(SKY_ANG / 2); }
                    while (dt < -(float)(SKY_ANG / 4)) { dt += (float)(SKY_ANG / 2); }
                    {
                        const float ang = dt * (2.0f * SKY_PI / (float)SKY_ANG);
                        const float across = rp * sinf(ang) / 8.5f;
                        const float along = rp / 38.0f;
                        v += 1.9f * expf(-across * across) * expf(-along * along * along);
                    }
                }
            }

            /* faint gas between the arms, so the disc is not two ribbons */
            v += 0.13f;

            v *= env;
            if (v < 0.0f) {
                v = 0.0f;
            }

            /* lumpiness, two octaves, multiplicative so it never lights up
             * empty sky */
            {
                const float fr = (float)rr * ((float)(NOISE_R - 1) / (float)NEB_R);
                const float fa = (float)p * ((float)NOISE_A / (float)NEB_A);
                const float n = 0.62f * noise_at(noise, fr, fa) +
                                0.38f * noise_at(noise, fr * 2.3f, fa * 2.7f);
                v *= 0.45f + 1.25f * n;
            }

            tmp[(size_t)rr * NEB_A + (size_t)p] = v;
        }
    }

    /* Star-forming knots, splatted along the arms. These are what the colour
     * table turns pink: brightness picks the hue, so a knot is an HII region
     * without needing a second channel through the inner loop. */
    for (i = 0; i < 30; i++) {
        const float th = 0.7f + frnd() * 4.2f;
        const float kr = ARM_A * expf(ARM_B * th);
        if (kr > 116.0f) {
            continue;
        }
        {
            const int krow = clampi((int)lrintf(kr / (SKY_CORNER / 255.0f)) / 4, 1, NEB_R - 2);
            const float kpsi = (float)((i % 2) * (SKY_ANG / 2)) + rnd2() * 26.0f;
            const int kcell = ((int)lrintf(kpsi / 4.0f) + NEB_A) & NEB_A_MASK;
            const float amp = 0.55f + frnd() * 1.35f;
            const int spread = 2 + (int)(frnd() * 3.0f);
            int dr, da;
            for (dr = -spread; dr <= spread; dr++) {
                const int row = krow + dr;
                if (row < 0 || row >= NEB_R) {
                    continue;
                }
                for (da = -spread * 2; da <= spread * 2; da++) {
                    const float q = (float)(dr * dr) / (float)(spread * spread) +
                                    (float)(da * da) / (float)(spread * spread * 4);
                    if (q > 1.0f) {
                        continue;
                    }
                    {
                        const int cell = (kcell + da + NEB_A) & NEB_A_MASK;
                        tmp[(size_t)row * NEB_A + (size_t)cell] += amp * (1.0f - q) *
                            ((float)s_nebg[clampi(row * 4 + 2, 0, 255)] / 255.0f);
                    }
                }
            }
        }
    }

    /* A 3x3 mean, wrapped in phase. The field is read at 2.7 px a row and the
     * dither only hides so much; this is what keeps the cells from printing
     * as facets when the whole thing turns. */
    for (rr = 0; rr < NEB_R; rr++) {
        for (p = 0; p < NEB_A; p++) {
            float sum = 0.0f;
            int n = 0, dr, da;
            for (dr = -1; dr <= 1; dr++) {
                const int row = rr + dr;
                if (row < 0 || row >= NEB_R) {
                    continue;
                }
                for (da = -1; da <= 1; da++) {
                    sum += tmp[(size_t)row * NEB_A + (size_t)((p + da + NEB_A) & NEB_A_MASK)];
                    n++;
                }
            }
            {
                const float v = sum / (float)n;
                s_neb[(size_t)rr * NEB_A + (size_t)p] = f2u8(v * 0.60f);
            }
        }
    }
}

static void put_star(int n, float rad_px, float ang_units, unsigned pop,
                     unsigned col, float bright, bool big)
{
    sky_star_t *st = &s_stars[n];
    int a = (int)lrintf(ang_units * 64.0f);   /* 1024ths of a turn -> 65536ths */
    st->rad_q4 = (uint16_t)clampi((int)lrintf(rad_px * 16.0f), 0, 4095);
    st->ang    = (uint16_t)((uint32_t)a & 0xFFFFu);
    st->bright = f2u8(bright);
    /* Twinkle rates land between about 3 and 13 seconds. The modulus in
     * render_stars() makes every one of these divide the wrap exactly, so a
     * star's twinkle is continuous across it rather than jumping once every
     * four minutes. */
    st->rate   = (uint8_t)(20 + (rnd_u32() % 61u));
    st->phase  = (uint8_t)(rnd_u32() & 0xFFu);
    st->kind   = (uint8_t)(pop | (col << 2) | (big ? 0x10u : 0u));
}

/*
 * The star table, in three populations because a galaxy is three populations.
 *
 * The bulge is old, warm and tightly packed; the arms are young, blue-white
 * and sampled off the spiral the gas uses, so stars and gas agree; the halo
 * is faint, old and everywhere, and it is the halo that makes the disc sit in
 * space rather than float on a black rectangle.
 */
static void build_stars(void)
{
    const int n_bulge = (SKY_STARS * 26) / 100;
    const int n_arm   = (SKY_STARS * 52) / 100;
    int n = 0;
    int guard;

    for (; n < n_bulge; n++) {
        const float r = fabsf(rnd2()) * 27.0f;
        /* The bulge is stretched along the bar, which is at screen angle zero
         * in the field above; the same elongation, or the bar would be gas
         * with no stars in it. */
        const float a = frnd() * (float)SKY_ANG;
        const float el = 1.0f + 0.95f * cosf(a * (2.0f * SKY_PI / (float)SKY_ANG) * 2.0f);
        /* Faint where they are densest, and gone at the very middle.
         * A quarter of the bulge falls inside three pixels of centre, which
         * as point sources adds up to a blown white dot -- at dead centre,
         * exactly where the clock and the album art go. The nucleus is the
         * core ramp's job; these are the stars around it. */
        const float u = r > 16.0f ? 1.0f : r / 16.0f;
        const float near = u * u * sqrtf(u);
        put_star(n, r * el, a, POP_BULGE, COL_CORE,
                 (0.42f + frnd() * 0.52f) * near, frnd() < 0.03f);
    }

    for (guard = 0; n < n_bulge + n_arm && guard < SKY_STARS * 8; guard++) {
        const int arm = n & 1;
        const float th = 0.55f + powf(frnd(), 0.55f) * 4.1f;
        const float ar = ARM_A * expf(ARM_B * th);
        if (ar > 117.0f) {
            continue;
        }
        {
            /* Scatter across the arm, not along it: an arm is a ribbon, and
             * scatter along it only makes the ribbon longer. */
            const float across = rnd2() * (3.0f + ar * 0.085f);
            const float rr = ar + rnd2() * 2.6f;
            const float ridge = (float)s_lnterm[clampi((int)lrintf(ar / (SKY_CORNER / 255.0f)), 0, 255)];
            const float off = across * ((float)SKY_ANG / (2.0f * SKY_PI)) / (ar + 9.0f);
            const float a = ridge + (float)(arm * (SKY_ANG / 2)) + off;
            const bool hii = frnd() < 0.12f;
            put_star(n, rr, a, POP_ARM,
                     hii ? COL_HII : (frnd() < 0.10f ? COL_ACCENT : COL_STAR),
                     0.34f + frnd() * 0.62f, frnd() < 0.055f);
            n++;
        }
    }

    for (; n < SKY_STARS; n++) {
        put_star(n, sqrtf(frnd()) * 118.0f, frnd() * (float)SKY_ANG,
                 POP_HALO, COL_STAR, 0.12f + frnd() * 0.34f, false);
    }

    /* The few the eye actually lands on. They carry a halo and the slowest
     * pulse on the screen, which is what stops the field from reading as
     * uniform grain. */
    for (n = 0; n < SKY_BEACONS; n++) {
        const float r = 22.0f + frnd() * 74.0f;
        s_beacon[n].rad_q4 = (uint16_t)lrintf(r * 16.0f);
        s_beacon[n].ang    = (uint16_t)(rnd_u32() & 0xFFFFu);
        s_beacon[n].col    = (uint8_t)((rnd_u32() & 1u) ? COL_CORE : COL_STAR);
        s_beacon[n].halo   = (uint8_t)(5 + (rnd_u32() % 4u));
        s_beacon[n].rate   = (uint8_t)(6 + (rnd_u32() % 9u));
        s_beacon[n].phase  = (uint8_t)(rnd_u32() & 0xFFu);
    }
}

static void *sky_alloc(size_t bytes)
{
#ifdef ESP_PLATFORM
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(bytes);
#endif
}

static void sky_free_all(void)
{
    free(s_map);
    free(s_neb);
    free(s_stars);
    s_map = NULL;
    s_neb = NULL;
    s_stars = NULL;
    s_ready = false;
}

/* One line, on the platform that has a logger; nothing on the host. */
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define SKY_REPORT_FOOTPRINT(kept, scratch)                                   \
    ESP_LOGI("hk_sky", "tables %u B kept + %u B scratch, in psram; "           \
                       "plus %u B of internal .bss",                           \
             (unsigned)(kept), (unsigned)(scratch), (unsigned)SKY_BSS_BYTES)
#else
#define SKY_REPORT_FOOTPRINT(kept, scratch) ((void)(kept), (void)(scratch))
#endif

/* Measured with xtensa-esp32s3-elf-size on this translation unit: 14,800 B of
 * .bss. Written down rather than computed, because the sum of the tables below
 * is not the same number the linker produces and only one of them is true. */
#define SKY_BSS_BYTES 14800u

bool hk_sky_init(uint32_t seed)
{
    float *tmp;

    sky_free_all();
    build_trig();
    s_rng = seed ? seed : 0x9E3779B9u;

    s_map   = (uint16_t *)sky_alloc(sizeof(uint16_t) * SKY_HALF * SKY_HALF);
    s_neb   = (uint8_t *)sky_alloc((size_t)NEB_R * NEB_A);
    s_stars = (sky_star_t *)sky_alloc(sizeof(sky_star_t) * SKY_STARS);
    tmp     = (float *)sky_alloc(sizeof(float) * (NEB_R * NEB_A + NOISE_R * NOISE_A));
    if (s_map == NULL || s_neb == NULL || s_stars == NULL || tmp == NULL) {
        free(tmp);
        sky_free_all();
        return false;
    }

    /* Say what it cost, rather than leaving a number in a header to go stale.
     *
     * The frozen header guessed "about 40 KB of PSRAM" and said nothing about
     * internal RAM; both were wrong -- the tables below are larger than that,
     * and the file also carries about 14.5 KiB of .bss that never leaves
     * internal memory. On a part where internal RAM is the scarce kind, a
     * footprint nobody prints is a footprint nobody can attribute. */
    SKY_REPORT_FOOTPRINT(sizeof(uint16_t) * SKY_HALF * SKY_HALF
                             + (size_t)NEB_R * NEB_A
                             + sizeof(sky_star_t) * SKY_STARS,
                         sizeof(float) * (NEB_R * NEB_A + NOISE_R * NOISE_A));

    build_map();
    build_radial();
    build_boot();
    build_nebula(tmp, tmp + NEB_R * NEB_A);
    build_stars();
    free(tmp);

    s_ready = true;
    return true;
}

/* ------------------------------------------------------------------ */
/* the breath                                                          */
/* ------------------------------------------------------------------ */

/*
 * Three periods, in milliseconds. All three are prime, so their least common
 * multiple is their product -- fifty-eight years -- and none of them can
 * quietly divide into another.
 *
 * That is the easy half. The half that matters is the NEAR repeat: three
 * periods can be coprime and still nearly realign after a couple of minutes,
 * and a couple of minutes is a length the eye does learn. These ratios sit
 * either side of the golden ratio, which is the spacing whose rational
 * approximations converge slowest, and they were then chosen by measurement
 * -- the closest the sum comes to matching a shifted copy of itself, anywhere
 * in the first hour, is a mean error of 3 in 255. One sine is recognised as a
 * loop inside ten seconds and stops feeling alive.
 */
#define BREATH_P1 7573u
#define BREATH_P2 12227u
#define BREATH_P3 19793u

static int32_t osc(uint32_t t_ms, uint32_t period)
{
    const uint32_t ph = (uint32_t)((((uint64_t)(t_ms % period)) * SKY_ANG) / period);
    return s_sin[ph & SKY_ANG_MASK];
}

uint8_t hk_sky_breath(uint32_t t_ms)
{
    int32_t acc;
    build_trig();
    acc = (121 * osc(t_ms, BREATH_P1) +
            84 * osc(t_ms, BREATH_P2) +
            49 * osc(t_ms, BREATH_P3)) >> 15;   /* -254 .. +254 */
    return (uint8_t)clampi(128 + acc / 2, 0, 255);
}

/* ------------------------------------------------------------------ */
/* the per-frame tables                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t rot_pat;          /* pattern rotation, 65536ths of a turn      */
    uint32_t rot_halo;
    uint32_t rot_bulge[32];    /* by radius, 4 px a step                    */
    uint32_t scale_q8;         /* boot expansion, 256 = settled             */
    uint32_t gain_q8;
} sky_frame_t;

static uint32_t turn16(uint32_t t_ms, uint32_t period_ms)
{
    if (period_ms == 0u) {
        return 0u;
    }
    return (uint32_t)((((uint64_t)(t_ms % period_ms)) << 16) / period_ms);
}

/* Between two samples of the boot curve; t beyond the table holds the last
 * value, which is the settled state. */
static uint32_t boot_lerp(const uint16_t *tab, uint32_t t_ms)
{
    const uint32_t i = t_ms / BOOT_STEP_MS;
    if (i >= (uint32_t)(BOOT_STEPS - 1)) {
        return tab[BOOT_STEPS - 1];
    }
    {
        /* Signed, because one of these three curves falls: the expansion runs
         * 666 down to 256, and an unsigned difference turns a step of -41 into
         * four billion. The frames that landed on a table entry looked right,
         * so the ones between them collapsed the whole radial map to r = 0 --
         * a flat lit rectangle, corners and all -- rather than crashing. */
        const int32_t f = (int32_t)(t_ms - i * BOOT_STEP_MS);
        const int32_t a = (int32_t)tab[i];
        const int32_t b = (int32_t)tab[i + 1];
        return (uint32_t)(a + (b - a) * f / BOOT_STEP_MS);
    }
}

static void pal_rgb(uint8_t out[3], uint16_t c, const sky_mood_cfg_t *m,
                    uint32_t scale_q8)
{
    int v[3];
    int grey;
    int i;
    v[0] = UNP_R(c);
    v[1] = UNP_G(c);
    v[2] = UNP_B(c);
    grey = (v[0] * 77 + v[1] * 150 + v[2] * 29) >> 8;
    for (i = 0; i < 3; i++) {
        v[i] = grey + ((v[i] - grey) * (int)m->sat >> 8);
    }
    /* Warmth in proportion to how much light there is. A flat shift lands
     * on deep space as well as on the arms, and EMBER stops being a warm
     * galaxy and becomes a red screen. */
    v[0] += m->warm * grey / 255;
    v[2] -= m->warm * grey / 255;
    for (i = 0; i < 3; i++) {
        out[i] = (uint8_t)clampi((int)(((int64_t)clampi(v[i], 0, 255) * (int)scale_q8) >> 8),
                                 0, 255);
    }
}

static void lerp_rgb(uint8_t out[3], const uint8_t a[3], const uint8_t b[3], int t)
{
    int i;
    for (i = 0; i < 3; i++) {
        out[i] = (uint8_t)(a[i] + (((int)b[i] - (int)a[i]) * t >> 8));
    }
}

static void build_frame(sky_frame_t *fr, uint32_t t_ms, hk_sky_mood_t mood, uint8_t level)
{
    const sky_mood_cfg_t *m = &s_moods[mood];
    uint8_t c_void[3], c_haze[3], c_aura[3], c_core[3], c_shell[3];
    uint8_t c_lo[3], c_hi[3], c_warm[3];
    uint32_t base_q8, inv_scale, inv_aura, inv_core, warp_pat;
    unsigned breath, aura_q8, core_q8;
    int rd, i;

    breath = hk_sky_breath(t_ms);

    fr->scale_q8 = 256u;
    base_q8 = (uint32_t)m->gain * level / 255u;
    if (mood == HK_SKY_BOOT) {
        fr->scale_q8 = boot_lerp(s_boot_scale, t_ms);
        base_q8 = base_q8 * boot_lerp(s_boot_gain, t_ms) / 256u;
    }
    fr->gain_q8 = base_q8;

    /* Rotation. The pattern -- arms, lanes, bar, gas -- turns rigidly; the
     * bulge runs away with it near the middle and the halo barely moves. That
     * is both what a galaxy does and the only arrangement in which the arms
     * are still arms after an hour of uptime. */
    {
        const uint32_t sp = m->spin ? m->spin : 1u;
        const uint32_t p_pat  = 78000u * 256u / sp;
        const uint32_t p_halo = p_pat * 9u / 2u;
        fr->rot_pat  = turn16(t_ms, p_pat);
        fr->rot_halo = turn16(t_ms, p_halo);
        for (i = 0; i < 32; i++) {
            const uint32_t r = (uint32_t)(i * 4 + 2);
            fr->rot_bulge[i] = turn16(t_ms, (19000u * 256u / sp) * (18u + r) / 18u);
        }
        if (mood == HK_SKY_BOOT) {
            const uint32_t extra = boot_lerp(s_boot_turn, t_ms);
            fr->rot_pat  = (fr->rot_pat + extra) & 0xFFFFu;
            fr->rot_halo = (fr->rot_halo + extra / 3u) & 0xFFFFu;
            for (i = 0; i < 32; i++) {
                fr->rot_bulge[i] = (fr->rot_bulge[i] + extra * 2u) & 0xFFFFu;
            }
        }
    }
    warp_pat = (fr->rot_pat >> 6) & SKY_ANG_MASK;

    /* The aura swells and the core answers it out of phase, so the middle
     * does not simply get brighter and dimmer as one lamp. */
    aura_q8 = 230u + breath * 56u / 255u;
    core_q8 = 286u - breath * 56u / 255u;

    pal_rgb(c_void,  HK_C_VOID,      m, base_q8);
    pal_rgb(c_haze,  HK_C_DEEP,      m, base_q8);
    pal_rgb(c_core,  HK_C_CORE,      m, base_q8 * 108u / 256u * m->core / 256u);
    pal_rgb(c_shell, HK_C_SHELL,     m, base_q8);
    pal_rgb(c_lo,    HK_C_NEBULA,    m, base_q8 * m->neb / 256u);
    pal_rgb(c_hi,    HK_C_DUST,      m, base_q8 * m->neb / 256u);
    pal_rgb(c_warm,  HK_C_CORE,      m, base_q8 * m->neb / 256u);

    /* The aura's hue wanders between the arm violet and the star-forming pink
     * on a two-minute clock, so the mood moves even when the structure does
     * not. */
    {
        uint8_t a[3], b[3];
        const int w = 128 + (osc(t_ms, 121003u) >> 8);
        pal_rgb(a, HK_C_NEBULA, m, base_q8 * 77u / 256u);
        pal_rgb(b, HK_C_DUST,   m, base_q8 * 77u / 256u);
        lerp_rgb(c_aura, a, b, clampi(w * 140 / 255, 0, 255));
    }

    inv_scale = (256u << 16) / fr->scale_q8;
    inv_aura  = (256u << 16) / aura_q8;
    inv_core  = (256u << 16) / core_q8;

    for (rd = 0; rd < SKY_ROWS; rd++) {
        const unsigned rs = (unsigned)clampi((int)(((uint32_t)rd * inv_scale) >> 16), 0, 255);
        const unsigned vig  = s_vig[rs];
        const unsigned nebg = (unsigned)s_nebg[rs] * vig >> 8;
        int base[3], lo[3], hi[3];
        uint16_t *row = &s_mix[(size_t)rd * MIX_LEVELS];
        int k;

        s_aux[rd] = ((uint32_t)((rs >> 2) * NEB_A) << 16) |
                    (((uint32_t)(0u - s_lnterm[rs] - warp_pat)) & SKY_ANG_MASK);

        {
            const unsigned haze = s_haze[rs];
            const unsigned aur  = s_aura[(unsigned)clampi((int)((rs * inv_aura) >> 16), 0, 255)];
            const unsigned cor  = s_core[(unsigned)clampi((int)((rs * inv_core) >> 16), 0, 255)];
            uint8_t l[3], h[3];
            lerp_rgb(l, c_lo, c_warm, s_nebw[rs]);
            lerp_rgb(h, c_hi, c_warm, s_nebw[rs] / 2);
            for (i = 0; i < 3; i++) {
                int v = c_void[i] + ((int)c_haze[i] * (int)haze >> 8) +
                        ((int)c_aura[i] * (int)aur >> 8) +
                        ((int)c_core[i] * (int)cor >> 8);
                v = clampi(v, 0, 255);
                base[i] = c_shell[i] + ((v - (int)c_shell[i]) * (int)vig >> 8);
                lo[i] = (int)l[i] * (int)nebg >> 8;
                hi[i] = (int)h[i] * (int)nebg >> 8;
            }
        }

        if (nebg == 0u) {
            const uint16_t c = (uint16_t)(((uint32_t)(base[0] & 0xF8) << 8) |
                                          ((uint32_t)(base[1] & 0xFC) << 3) |
                                          ((uint32_t)base[2] >> 3));
            for (k = 0; k < MIX_LEVELS; k++) {
                row[k] = c;
            }
            continue;
        }

        for (k = 0; k < MIX_LEVELS; k++) {
            const int t = k * 16;              /* 0..256, the nebula level   */
            int c[3];
            for (i = 0; i < 3; i++) {
                /* faint gas is violet, a bright knot is pink: intensity
                 * chooses the hue, which is what lets one 8-bit field carry
                 * both without a second lookup in the inner loop */
                const int tc = (t * 168) >> 8;
                const int col = (lo[i] * (256 - tc) + hi[i] * tc) >> 8;
                c[i] = clampi(base[i] + ((col * t) >> 8), 0, 255);
            }
            row[k] = (uint16_t)(((uint32_t)(c[0] & 0xF8) << 8) |
                                ((uint32_t)(c[1] & 0xFC) << 3) |
                                ((uint32_t)c[2] >> 3));
        }
    }
}

/* ------------------------------------------------------------------ */
/* the ground: every pixel, written exactly once                       */
/* ------------------------------------------------------------------ */

/*
 * Four loads and a store.
 *
 * The map gives radius and quadrant angle together; the quadrant's base and
 * sign turn that into a screen angle; the frame's warp table turns the screen
 * angle into an arm phase, which is where the nebula is stored, so the whole
 * galaxy turns by an integer addition. The colour table has already been told
 * what mood it is, how bright the screen is, where the breath is and how far
 * through the opening we are -- which is why none of that appears here.
 */
static inline uint16_t shade(uint16_t m, int qbase, int sgn, unsigned d)
{
    const unsigned rd  = (unsigned)(m >> 8) + (d >> 2);
    const uint32_t aux = s_aux[rd];
    const unsigned th  = (unsigned)(qbase + sgn * (int)(m & 0xFFu)) +
                         (unsigned)(aux & SKY_ANG_MASK);
    const unsigned n   = s_neb[(aux >> 16) + ((th >> 2) & NEB_A_MASK)];
    return s_mix[rd * MIX_LEVELS + ((n + d) >> 4)];
}

static void render_ground(uint16_t *buf)
{
    int y;
    for (y = 0; y < SKY_H; y++) {
        const int top = (y < SKY_HALF);
        const int qy = top ? (SKY_HALF - 1 - y) : (y - SKY_HALF);
        const uint16_t *mrow = &s_map[(size_t)qy * SKY_HALF];
        const uint8_t *dith = s_bayer[y & 3];
        int x;

        /* Left of centre the angle runs backwards through the quadrant, right
         * of it forwards; above and below swap which half of the turn they
         * are in. Four cases, two per row, both constant across the run. */
        {
            const int qb = 768, sg = top ? +1 : -1;
            for (x = 0; x < SKY_HALF; x++) {
                s_line[x] = shade(mrow[SKY_HALF - 1 - x], qb, sg, dith[x & 3]);
            }
        }
        {
            const int qb = 256, sg = top ? -1 : +1;
            for (x = SKY_HALF; x < SKY_W; x++) {
                s_line[x] = shade(mrow[x - SKY_HALF], qb, sg, dith[x & 3]);
            }
        }

        /* The row is assembled in internal RAM and handed over in one burst.
         * The framebuffer is in PSRAM: 240 scattered half-word stores are a
         * different cost from one 480-byte copy, and this pass has no reason
         * to read a single pixel back. */
        memcpy(&buf[(size_t)y * SKY_W], s_line, sizeof(s_line));
    }
}

/* ------------------------------------------------------------------ */
/* the stars: the only thing here that blends                          */
/* ------------------------------------------------------------------ */

static void add_px(uint16_t *buf, int x, int y, const uint8_t *c, unsigned w)
{
    uint16_t *p;
    unsigned r, g, b;
    uint16_t d;

    if (w == 0u || x < 0 || y < 0 || x >= SKY_W || y >= SKY_H) {
        return;
    }
    p = &buf[(size_t)y * SKY_W + (size_t)x];
    d = *p;
    /* Additive, not source-over: a star is light arriving, and over the
     * nebula the difference is the whole point. */
    /* Rounded, not truncated. Green has six bits and red and blue five, so
     * a truncating add gives green a step where the other two still round to
     * nothing -- and every faint glow on the screen comes out green. */
    r = (unsigned)(d >> 11) + ((c[0] * w + 1024u) >> 11);
    g = (unsigned)((d >> 5) & 63u) + ((c[1] * w + 512u) >> 10);
    b = (unsigned)(d & 31u) + ((c[2] * w + 1024u) >> 11);
    if (r > 31u) { r = 31u; }
    if (g > 63u) { g = 63u; }
    if (b > 31u) { b = 31u; }
    *p = (uint16_t)((r << 11) | (g << 5) | b);
}

/* Sub-pixel, because a star that snaps to the pixel grid stops moving
 * between frames and the rotation goes with it. */
static void splat(uint16_t *buf, int xq, int yq, const uint8_t *c, unsigned amp)
{
    const int px = xq >> 4;
    const int py = yq >> 4;
    const unsigned fx = (unsigned)(xq & 15);
    const unsigned fy = (unsigned)(yq & 15);
    const unsigned gx = 16u - fx;
    const unsigned gy = 16u - fy;
    add_px(buf, px,     py,     c, amp * gx * gy >> 8);
    add_px(buf, px + 1, py,     c, amp * fx * gy >> 8);
    add_px(buf, px,     py + 1, c, amp * gx * fy >> 8);
    add_px(buf, px + 1, py + 1, c, amp * fx * fy >> 8);
}

static void render_stars(uint16_t *buf, const sky_frame_t *fr, uint32_t t_ms,
                         const sky_mood_cfg_t *m)
{
    /* 262 144 ms. Every twinkle rate divides this wrap exactly -- (P*rate)>>8
     * is a multiple of 1024 for any integer rate -- so the phase is continuous
     * across it instead of jumping every four minutes. */
    const uint32_t tt = t_ms & 0x3FFFFu;
    const uint32_t gain = fr->gain_q8 * m->star / 256u;
    int i;

    pal_rgb(s_starcol[COL_CORE],   HK_C_CORE,      m, 256u);
    pal_rgb(s_starcol[COL_STAR],   HK_C_STARLIGHT, m, 256u);
    pal_rgb(s_starcol[COL_HII],    HK_C_DUST,      m, 256u);
    pal_rgb(s_starcol[COL_ACCENT], HK_C_ACCENT,    m, 256u);

    for (i = 0; i < SKY_STARS; i++) {
        const sky_star_t *st = &s_stars[i];
        const int rad = (int)(((uint32_t)st->rad_q4 * fr->scale_q8) >> 8);
        const unsigned pop = st->kind & 3u;
        uint32_t rot, ang, ph;
        int xq, yq, idx;
        unsigned amp;

        /* Still falling in from off the panel, during the opening. */
        if (rad > 1968) {
            continue;
        }

        if (pop == POP_ARM) {
            rot = fr->rot_pat;
        } else if (pop == POP_HALO) {
            rot = fr->rot_halo;
        } else {
            rot = fr->rot_bulge[clampi(rad >> 6, 0, 31)];
        }

        ph = ((((uint32_t)tt * st->rate) >> 8) + (uint32_t)st->phase * 4u) & SKY_ANG_MASK;
        amp = ((unsigned)st->bright * (unsigned)(153 + (s_sin[ph] * 102 >> 15))) >> 8;
        amp = amp * gain >> 8;
        if (amp == 0u) {
            continue;
        }
        if (amp > 255u) {
            amp = 255u;
        }

        ang = ((uint32_t)st->ang + rot) & 0xFFFFu;
        idx = (int)(ang >> 6);
        xq = SKY_C_Q4 + (int)(((int32_t)s_sin[idx] * rad) >> 15);
        yq = SKY_C_Q4 - (int)(((int32_t)s_sin[(idx + 256) & SKY_ANG_MASK] * rad) >> 15);
        if (xq < 0 || yq < 0 || xq >= (SKY_W - 1) * 16 || yq >= (SKY_H - 1) * 16) {
            continue;
        }

        {
            const uint8_t *c = s_starcol[(st->kind >> 2) & 3u];
            splat(buf, xq, yq, c, amp);
            if (st->kind & 0x10u) {
                /* The handful that are meant to be seen as objects rather
                 * than as grain get one ring of spill. */
                const int px = xq >> 4;
                const int py = yq >> 4;
                const unsigned h = amp / 3u;
                const unsigned q = amp / 7u;
                add_px(buf, px - 1, py,     c, h);
                add_px(buf, px + 2, py,     c, h);
                add_px(buf, px,     py - 1, c, h);
                add_px(buf, px,     py + 2, c, h);
                add_px(buf, px + 1, py - 1, c, q);
                add_px(buf, px - 1, py + 1, c, q);
                add_px(buf, px + 2, py + 1, c, q);
                add_px(buf, px + 1, py + 2, c, q);
            }
        }
    }

    for (i = 0; i < SKY_BEACONS; i++) {
        const sky_beacon_t *bc = &s_beacon[i];
        const int rad = (int)(((uint32_t)bc->rad_q4 * fr->scale_q8) >> 8);
        const uint8_t *c = s_starcol[bc->col];
        uint32_t ang, ph;
        int xq, yq, idx, px, py, dx, dy, rr;
        unsigned amp, pulse;

        if (rad > 1968) {
            continue;
        }
        ang = ((uint32_t)bc->ang + fr->rot_pat) & 0xFFFFu;
        idx = (int)(ang >> 6);
        xq = SKY_C_Q4 + (int)(((int32_t)s_sin[idx] * rad) >> 15);
        yq = SKY_C_Q4 - (int)(((int32_t)s_sin[(idx + 256) & SKY_ANG_MASK] * rad) >> 15);
        if (xq < 0 || yq < 0 || xq >= (SKY_W - 1) * 16 || yq >= (SKY_H - 1) * 16) {
            continue;
        }
        ph = ((((uint32_t)tt * bc->rate) >> 8) + (uint32_t)bc->phase * 4u) & SKY_ANG_MASK;
        pulse = (unsigned)(140 + (s_sin[ph] * 115 >> 15));
        amp = (255u * pulse >> 8) * gain >> 8;
        if (amp > 255u) {
            amp = 255u;
        }

        px = xq >> 4;
        py = yq >> 4;
        rr = (int)bc->halo;
        for (dy = -rr; dy <= rr; dy++) {
            for (dx = -rr; dx <= rr; dx++) {
                const int d2 = dx * dx + dy * dy;
                if (d2 > rr * rr) {
                    continue;
                }
                add_px(buf, px + dx, py + dy, c,
                       (amp * s_halo[(d2 * 64) / (rr * rr)] >> 8) / 7u);
            }
        }
        splat(buf, xq, yq, c, amp);
    }
}

/* ------------------------------------------------------------------ */

static uint64_t now_us(void)
{
#ifdef ESP_PLATFORM
    return (uint64_t)esp_timer_get_time();
#else
    return 0u;
#endif
}

void hk_sky_render(uint16_t *buf, uint32_t t_ms, hk_sky_mood_t mood, uint8_t level)
{
    const uint64_t t0 = now_us();
    sky_frame_t fr;

    if (buf == NULL) {
        return;
    }
    if ((unsigned)mood >= (unsigned)HK_SKY_MOOD_COUNT) {
        mood = HK_SKY_GALAXY;
    }
    if (!s_ready) {
        /* Never leave the caller with an uninitialised framebuffer: a screen
         * drawn over noise is harder to diagnose than one drawn over space. */
        hk_draw_fill(buf, HK_C_VOID);
        s_last_us = (uint32_t)(now_us() - t0);
        return;
    }

    build_frame(&fr, t_ms, mood, level);
    render_ground(buf);
    render_stars(buf, &fr, t_ms, &s_moods[mood]);
    s_last_us = (uint32_t)(now_us() - t0);
}

uint32_t hk_sky_last_us(void)
{
    return s_last_us;
}
