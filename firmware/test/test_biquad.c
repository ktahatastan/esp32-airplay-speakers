#include "hk_test.h"
#include "hk_biquad.h"

#include <math.h>

/*
 * The corner frequencies used here are placeholders. The real ones come from
 * G0 and G2 and do not exist yet; nothing in this file is a tuning decision.
 *
 * What IS being checked is the maths, and it is checked against the frequency
 * response rather than against a table of expected coefficients. A coefficient
 * table would pass just as happily with the wrong formula transcribed
 * consistently; the response cannot.
 */

#define FS 48000.0f
#define FC 2000.0f

/** |H(e^jw)| for one section, from its coefficients. */
static void section_response(const hk_biquad_coeffs_t *c, float w,
                             float *re, float *im)
{
    const float c1 = cosf(w), s1 = sinf(w);
    const float c2 = cosf(2.0f * w), s2 = sinf(2.0f * w);

    const float nr = c->b0 + c->b1 * c1 + c->b2 * c2;
    const float ni = -(c->b1 * s1 + c->b2 * s2);
    const float dr = 1.0f + c->a1 * c1 + c->a2 * c2;
    const float di = -(c->a1 * s1 + c->a2 * s2);

    const float den = dr * dr + di * di;
    *re = (nr * dr + ni * di) / den;
    *im = (ni * dr - nr * di) / den;
}

/** Two identical cascaded sections: H^2. */
static void lr4_response(const hk_lr4_coeffs_t *f, float w, float *re, float *im)
{
    float r, i;
    section_response(&f->section[0], w, &r, &i);
    float r2, i2;
    section_response(&f->section[1], w, &r2, &i2);
    *re = r * r2 - i * i2;
    *im = r * i2 + i * r2;
}

static float magnitude(float re, float im)
{
    return sqrtf(re * re + im * im);
}

void test_biquad(void)
{
    hk_biquad_coeffs_t lp, hp;
    hk_lr4_coeffs_t lr_lo, lr_hi;

    /* ===== a request that cannot describe a filter is refused ===== */
    HK_CHECK(hk_biquad_lowpass(&lp, FC, FS, HK_BIQUAD_Q_BUTTERWORTH));
    HK_CHECK(!hk_biquad_lowpass(NULL, FC, FS, 0.7f));
    HK_CHECK(!hk_biquad_lowpass(&lp, 0.0f, FS, 0.7f));       /* no corner */
    HK_CHECK(!hk_biquad_lowpass(&lp, -100.0f, FS, 0.7f));
    HK_CHECK(!hk_biquad_lowpass(&lp, FC, 0.0f, 0.7f));       /* no sample rate */
    HK_CHECK(!hk_biquad_lowpass(&lp, FC, FS, 0.0f));         /* no Q */
    HK_CHECK(!hk_biquad_lowpass(&lp, FC, FS, -1.0f));
    HK_CHECK(!hk_biquad_lowpass(&lp, NAN, FS, 0.7f));
    /* At or above Nyquist there is no such filter. */
    HK_CHECK(!hk_biquad_lowpass(&lp, FS / 2.0f, FS, 0.7f));
    HK_CHECK(!hk_biquad_lowpass(&lp, FS, FS, 0.7f));
    HK_CHECK(hk_biquad_lowpass(&lp, FS / 2.0f - 1.0f, FS, 0.7f));
    /* the high-pass refuses on the same terms */
    HK_CHECK(!hk_biquad_highpass(&hp, FS / 2.0f, FS, 0.7f));
    HK_CHECK(!hk_biquad_highpass(&hp, FC, FS, 0.0f));

    /* ===== a second-order Butterworth section ===== */
    HK_CHECK(hk_biquad_lowpass(&lp, FC, FS, HK_BIQUAD_Q_BUTTERWORTH));
    HK_CHECK(hk_biquad_highpass(&hp, FC, FS, HK_BIQUAD_Q_BUTTERWORTH));
    HK_CHECK(hk_biquad_stable(&lp));
    HK_CHECK(hk_biquad_stable(&hp));

    {
        float re, im;
        /* Low-pass: unity at DC, nothing at Nyquist. */
        section_response(&lp, 0.0f, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 1.0f) < 1e-4f);
        section_response(&lp, (float)M_PI, &re, &im);
        HK_CHECK(magnitude(re, im) < 1e-4f);

        /* High-pass: the other way round. */
        section_response(&hp, 0.0f, &re, &im);
        HK_CHECK(magnitude(re, im) < 1e-4f);
        section_response(&hp, (float)M_PI, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 1.0f) < 1e-4f);

        /* And -3 dB at the corner, which is what Butterworth means. */
        const float w_c = 2.0f * (float)M_PI * FC / FS;
        section_response(&lp, w_c, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 0.70710678f) < 1e-3f);
        section_response(&hp, w_c, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 0.70710678f) < 1e-3f);
    }

    /* ===== Linkwitz-Riley 4th order ===== */
    HK_CHECK(hk_lr4_lowpass(&lr_lo, FC, FS));
    HK_CHECK(hk_lr4_highpass(&lr_hi, FC, FS));
    HK_CHECK(!hk_lr4_lowpass(&lr_lo, FS, FS));      /* same refusals */
    HK_CHECK(!hk_lr4_lowpass(NULL, FC, FS));
    HK_CHECK(!hk_lr4_highpass(NULL, FC, FS));
    HK_CHECK(hk_lr4_lowpass(&lr_lo, FC, FS));
    HK_CHECK(hk_lr4_highpass(&lr_hi, FC, FS));

    /* Both sections must be identical, and both stable. */
    HK_CHECK(lr_lo.section[0].b0 == lr_lo.section[1].b0);
    HK_CHECK(lr_lo.section[0].a1 == lr_lo.section[1].a1);
    HK_CHECK(hk_biquad_stable(&lr_lo.section[0]));
    HK_CHECK(hk_biquad_stable(&lr_hi.section[1]));

    {
        float re, im;
        const float w_c = 2.0f * (float)M_PI * FC / FS;

        /* -6 dB at the corner on each branch. This is the number that
         * distinguishes Linkwitz-Riley from a Butterworth pair, and it is why
         * the two branches add back to unity instead of to +3 dB. */
        lr4_response(&lr_lo, w_c, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 0.5f) < 1e-3f);
        lr4_response(&lr_hi, w_c, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 0.5f) < 1e-3f);

        /* Passband and stopband ends. */
        lr4_response(&lr_lo, 0.0f, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 1.0f) < 1e-4f);
        lr4_response(&lr_hi, (float)M_PI, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 1.0f) < 1e-4f);
    }

    /* ===== THE property: the two branches sum flat =====
     * This is what a crossover is for, and it is the test that would catch
     * almost any error in the coefficients — a wrong Q, a transposed sign, a
     * cascade that is not two identical sections. Checked across the whole
     * spectrum rather than at the corner, because a pair can be correct at the
     * corner and wrong either side of it. */
    for (int i = 1; i < 2000; i++) {
        const float w = (float)M_PI * (float)i / 2000.0f;
        float lo_re, lo_im, hi_re, hi_im;
        lr4_response(&lr_lo, w, &lo_re, &lo_im);
        lr4_response(&lr_hi, w, &hi_re, &hi_im);
        const float sum = magnitude(lo_re + hi_re, lo_im + hi_im);
        HK_CHECK(fabsf(sum - 1.0f) < 2e-3f);
    }

    /* And the trap worth naming: inverting one branch, which LR2 needs and LR4
     * does not, produces a deep notch at the corner rather than a flat sum. */
    {
        const float w_c = 2.0f * (float)M_PI * FC / FS;
        float lo_re, lo_im, hi_re, hi_im;
        lr4_response(&lr_lo, w_c, &lo_re, &lo_im);
        lr4_response(&lr_hi, w_c, &hi_re, &hi_im);
        const float inverted = magnitude(lo_re - hi_re, lo_im - hi_im);
        HK_CHECK(inverted < 0.1f);   /* a notch, as promised */
    }

    /* ===== the filters actually run ===== */
    {
        /* A steady input through a low-pass settles at the input. */
        hk_lr4_state_t st = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};
        float y = 0.0f;
        for (int i = 0; i < 20000; i++) {
            y = hk_lr4_process_one(&lr_lo, &st, 1.0f);
        }
        HK_CHECK(fabsf(y - 1.0f) < 1e-3f);

        /* The same input through a high-pass settles at nothing. */
        hk_lr4_state_t sh = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};
        for (int i = 0; i < 20000; i++) {
            y = hk_lr4_process_one(&lr_hi, &sh, 1.0f);
        }
        HK_CHECK(fabsf(y) < 1e-3f);
    }

    {
        /* An impulse decays rather than growing: the audible form of the
         * stability check above. */
        hk_lr4_state_t st = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};
        float peak_late = 0.0f;
        for (int i = 0; i < 50000; i++) {
            const float y = hk_lr4_process_one(&lr_lo, &st, (i == 0) ? 1.0f : 0.0f);
            HK_CHECK(!isnan(y));
            if (i > 5000 && fabsf(y) > peak_late) {
                peak_late = fabsf(y);
            }
        }
        HK_CHECK(peak_late < 1e-3f);
    }

    /* ===== the slope, measured through the filter that will actually run =====
     * Every response check above works from the COEFFICIENTS. That leaves a
     * gap the coefficients cannot see: if hk_lr4_process_one() ran only one of
     * its two sections, the maths would still be right and the crossover would
     * silently be 12 dB/octave instead of 24. The sum would stop being flat
     * and a tweeter would get twice the low-frequency energy it was designed
     * for. So this one drives a sine through the real processing path and
     * measures what comes out. */
    {
        const float f_test = FC * 4.0f;            /* two octaves above */
        const float w = 2.0f * (float)M_PI * f_test / FS;
        hk_lr4_state_t st = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};

        float peak = 0.0f;
        for (int i = 0; i < 40000; i++) {
            const float x = sinf(w * (float)i);
            const float y = hk_lr4_process_one(&lr_lo, &st, x);
            if (i > 20000 && fabsf(y) > peak) {    /* past the transient */
                peak = fabsf(y);
            }
        }

        /* Fourth order is 24 dB/octave, so two octaves down is about 48 dB:
         * roughly 0.004. A single second-order section would leave about
         * 24 dB, roughly 0.063 — an order of magnitude apart, so the bound
         * below separates them without being fussy about the exact figure. */
        HK_CHECK(peak < 0.015f);
        HK_CHECK(peak > 0.0005f);   /* and it is a filter, not a mute */
    }

    /* The high branch, the same way, two octaves below its corner. */
    {
        const float f_test = FC / 4.0f;
        const float w = 2.0f * (float)M_PI * f_test / FS;
        hk_lr4_state_t st = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};

        float peak = 0.0f;
        for (int i = 0; i < 60000; i++) {
            const float x = sinf(w * (float)i);
            const float y = hk_lr4_process_one(&lr_hi, &st, x);
            if (i > 30000 && fabsf(y) > peak) {
                peak = fabsf(y);
            }
        }
        HK_CHECK(peak < 0.015f);
        HK_CHECK(peak > 0.0005f);
    }

    /* ===== a corner low enough to matter for a tweeter's protection ===== */
    /* This is where Direct Form I would start losing bits in single precision,
     * so it is worth having a case that lives there. */
    {
        hk_lr4_coeffs_t sub;
        HK_CHECK(hk_lr4_highpass(&sub, 80.0f, 48000.0f));
        HK_CHECK(hk_biquad_stable(&sub.section[0]));

        float re, im;
        const float w_c = 2.0f * (float)M_PI * 80.0f / 48000.0f;
        lr4_response(&sub, w_c, &re, &im);
        HK_CHECK(fabsf(magnitude(re, im) - 0.5f) < 5e-3f);

        hk_lr4_state_t st = {{{0.0f, 0.0f}, {0.0f, 0.0f}}};
        float y = 0.0f;
        for (int i = 0; i < 200000; i++) {
            y = hk_lr4_process_one(&sub, &st, 1.0f);
            HK_CHECK(!isnan(y));
        }
        HK_CHECK(fabsf(y) < 1e-2f);   /* DC is removed */
    }

    /* ===== instability is recognised ===== */
    {
        hk_biquad_coeffs_t bad = {1.0f, 0.0f, 0.0f, 0.0f, 1.5f};  /* |a2| > 1 */
        HK_CHECK(!hk_biquad_stable(&bad));
        bad.a2 = 0.5f; bad.a1 = 2.0f;                             /* a1 too large */
        HK_CHECK(!hk_biquad_stable(&bad));
        bad.a1 = NAN;
        HK_CHECK(!hk_biquad_stable(&bad));
        HK_CHECK(!hk_biquad_stable(NULL));
    }

    /* ===== degenerate calls do not crash ===== */
    {
        hk_biquad_state_t st = {0.0f, 0.0f};
        HK_CHECK(hk_biquad_process_one(NULL, &st, 0.25f) == 0.25f);
        HK_CHECK(hk_biquad_process_one(&lp, NULL, 0.25f) == 0.25f);
        hk_biquad_process(&lp, &st, NULL, 4);
        hk_lr4_process(NULL, NULL, NULL, 4);
    }
}
