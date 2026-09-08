/**
 * @file test_gfx.c
 * @brief The compositor's two load-bearing promises.
 *
 * The first is containment. Every primitive writes through one clipped pixel
 * function, and the reason is that the framebuffer is 115,200 bytes in PSRAM
 * with the heap immediately after it: a write one row past the end does not
 * show a wrong pixel, it shows a corrupted allocation somewhere else, seconds
 * later, in whatever ran next. So the tests here fill the buffer with a
 * sentinel, draw shapes deliberately far outside their bounds, and assert the
 * sentinel survived everywhere it should have.
 *
 * The second is that alpha composites rather than creeps. RGB565 quantises
 * unevenly, and a blend that rounds the wrong way turns a static translucent
 * panel into one that slowly brightens over a few hundred frames.
 */
#include "hk_test.h"

#include <stdint.h>
#include <string.h>

#include "hk_draw.h"
#include "hk_gfx.h"

#define W HK_DRAW_WIDTH
#define H HK_DRAW_HEIGHT
#define SENTINEL 0xA55Au

static uint16_t buf[W * H];

static void reset(void)
{
    for (int i = 0; i < W * H; i++) {
        buf[i] = SENTINEL;
    }
    hk_gfx_clip_reset();
}

/** How many pixels outside the rectangle stopped being the sentinel. */
static int escaped(int x, int y, int w, int h)
{
    int count = 0;
    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            const bool inside = (px >= x && px < x + w && py >= y && py < y + h);
            if (!inside && buf[py * W + px] != SENTINEL) {
                count++;
            }
        }
    }
    return count;
}

void test_gfx(void)
{
    /* --- the clip holds, for every primitive ------------------------- */
    reset();
    hk_gfx_clip_set(60, 70, 40, 30);
    hk_gfx_disc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(200), 0x1234, 255);
    hk_gfx_ring(buf, HK_Q4(120), HK_Q4(120), HK_Q4(300), HK_Q4(4), 0x1234, 255);
    hk_gfx_arc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(300), HK_Q4(4), 0, 1000, 0x1234, 255);
    hk_gfx_rect(buf, -50, -50, 400, 400, 0x1234, 255);
    hk_gfx_rrect(buf, -50, -50, 400, 400, 20, 0x1234, 255);
    hk_gfx_rrect_outline(buf, -50, -50, 400, 400, 20, 3, 0x1234, 255);
    hk_gfx_px(buf, 5, 5, 0x1234, 255);
    hk_gfx_vignette(buf, 120, 120, 10, 200, 255);
    hk_gfx_dim(buf, 128);
    hk_gfx_iris(buf, 120, 120, HK_Q4(20), HK_Q4(8), 0x1234, true);
    HK_CHECK_EQ_INT(escaped(60, 70, 40, 30), 0);

    /* A clip cannot widen the buffer: asking for one outside it draws nothing
     * rather than wrapping to the opposite edge. */
    reset();
    hk_gfx_clip_set(-100, -100, 50, 50);
    hk_gfx_rect(buf, -200, -200, 800, 800, 0x1234, 255);
    HK_CHECK_EQ_INT(escaped(0, 0, 0, 0), 0);

    reset();
    hk_gfx_clip_set(W + 10, H + 10, 40, 40);
    hk_gfx_disc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(200), 0x1234, 255);
    HK_CHECK_EQ_INT(escaped(0, 0, 0, 0), 0);

    /* Reset means the whole buffer again, and no primitive may leave it. */
    reset();
    hk_gfx_rect(buf, -400, -400, 2000, 2000, 0x1234, 255);
    for (int i = 0; i < W * H; i++) {
        if (buf[i] == SENTINEL) {
            HK_CHECK(false);
            break;
        }
    }

    /* --- alpha is a composite, not a creep -------------------------- */
    HK_CHECK_EQ_INT(hk_gfx_blend(0x1234, 0xFFFF, 0), 0x1234);
    HK_CHECK_EQ_INT(hk_gfx_blend(0x1234, 0xFFFF, 255), 0xFFFF);
    HK_CHECK_EQ_INT(hk_gfx_mix(0x1234, 0xABCD, 0), 0x1234);
    HK_CHECK_EQ_INT(hk_gfx_mix(0x1234, 0xABCD, 255), 0xABCD);

    /* Zero alpha, applied a thousand times, must still be a no-op. If it
     * rounded up by one step per pass, a chip drawn every frame would be white
     * inside a minute. */
    uint16_t held = 0x39E7;
    for (int i = 0; i < 1000; i++) {
        held = hk_gfx_blend(held, 0xFFFF, 0);
    }
    HK_CHECK_EQ_INT(held, 0x39E7);

    /* Repeated partial blending must converge on the source and stop, not
     * oscillate around it. */
    uint16_t drifting = 0x0000;
    for (int i = 0; i < 500; i++) {
        drifting = hk_gfx_blend(drifting, 0x8410, 64);
    }
    const uint16_t settled = hk_gfx_blend(drifting, 0x8410, 64);
    HK_CHECK_EQ_INT(settled, drifting);

    /* --- an arc runs clockwise from twelve o'clock ------------------- */
    reset();
    hk_gfx_arc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(100), HK_Q4(90), 0, 250, 0x1234, 255);
    /* A quarter turn from the top covers the right side, not the left. Sampled
     * at half past one rather than at three o'clock: the sweep's far end is its
     * boundary, and a test that lands exactly on it is testing whether the edge
     * is inclusive rather than whether the arc runs the right way. */
    HK_CHECK(buf[(120 - 67) * W + (120 + 67)] != SENTINEL);  /* 1:30: drawn  */
    HK_CHECK(buf[(120 - 67) * W + (120 - 67)] == SENTINEL);  /* 10:30: not   */
    HK_CHECK(buf[(120 + 67) * W + (120 + 67)] == SENTINEL);  /* 4:30: not    */
    HK_CHECK(buf[120 * W + 25] == SENTINEL);                 /* nine: not    */

    /* Nothing, and everything, are both expressible. */
    reset();
    hk_gfx_arc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(100), HK_Q4(90), 0, 0, 0x1234, 255);
    HK_CHECK_EQ_INT(escaped(0, 0, 0, 0), 0);
    reset();
    hk_gfx_arc(buf, HK_Q4(120), HK_Q4(120), HK_Q4(100), HK_Q4(90), 0, 1000, 0x1234, 255);
    HK_CHECK(buf[120 * W + 25] != SENTINEL && buf[120 * W + 215] != SENTINEL);

    /* An inverted annulus draws nothing rather than filling the plane. */
    reset();
    hk_gfx_ring(buf, HK_Q4(120), HK_Q4(120), HK_Q4(20), HK_Q4(90), 0x1234, 255);
    HK_CHECK_EQ_INT(escaped(0, 0, 0, 0), 0);

    /* --- the iris keeps the side it was asked to keep ---------------- */
    reset();
    hk_gfx_iris(buf, 120, 120, HK_Q4(40), HK_Q4(4), 0x0000, true);
    HK_CHECK_EQ_INT(buf[120 * W + 120], SENTINEL);   /* centre survives */
    HK_CHECK(buf[5 * W + 5] != SENTINEL);            /* corner painted  */

    reset();
    hk_gfx_iris(buf, 120, 120, HK_Q4(40), HK_Q4(4), 0x0000, false);
    HK_CHECK(buf[120 * W + 120] != SENTINEL);        /* centre painted  */
    HK_CHECK_EQ_INT(buf[5 * W + 5], SENTINEL);       /* corner survives */

    /* --- a NULL buffer is a no-op, not a fault ---------------------- */
    hk_gfx_clip_reset();
    hk_gfx_disc(NULL, HK_Q4(10), HK_Q4(10), HK_Q4(5), 0x1234, 255);
    hk_gfx_rect(NULL, 0, 0, 10, 10, 0x1234, 255);
    hk_gfx_px(NULL, 0, 0, 0x1234, 255);
    HK_CHECK(true);
}
