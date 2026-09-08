#include "hk_test.h"
#include "hk_draw.h"

#include <string.h>

/*
 * The buffer is deliberately larger than the panel, with a guard band either
 * side. Every test then asserts the guards are untouched: a primitive that
 * writes one pixel past the end would corrupt the heap on the device, and the
 * symptom there would arrive seconds later and somewhere else.
 */
#define GUARD 64
#define PIXELS ((size_t)HK_DRAW_WIDTH * HK_DRAW_HEIGHT)

static uint16_t arena[GUARD + PIXELS + GUARD];
static uint16_t *fb = arena + GUARD;

static void reset(void)
{
    memset(arena, 0xAB, sizeof(arena));
    memset(fb, 0, PIXELS * sizeof(uint16_t));
}

static bool guards_intact(void)
{
    for (size_t i = 0; i < GUARD; i++) {
        if (arena[i] != 0xABAB) return false;
        if (arena[GUARD + PIXELS + i] != 0xABAB) return false;
    }
    return true;
}

static uint16_t at(int x, int y)
{
    return fb[(size_t)y * HK_DRAW_WIDTH + (size_t)x];
}

static size_t painted(void)
{
    size_t n = 0;
    for (size_t i = 0; i < PIXELS; i++) {
        if (fb[i] != 0) n++;
    }
    return n;
}

void test_draw(void)
{
    const uint16_t C = hk_rgb(255, 255, 255);

    /* ===== colour packing ===== */
    HK_CHECK(hk_rgb(0, 0, 0) == 0x0000);
    HK_CHECK(hk_rgb(255, 255, 255) == 0xFFFF);
    /* Red occupies the top five bits, blue the bottom five. */
    HK_CHECK((hk_rgb(255, 0, 0) & 0xF800) == 0xF800);
    HK_CHECK((hk_rgb(0, 0, 255) & 0x001F) == 0x001F);
    HK_CHECK(hk_rgb_scaled(255, 255, 255, 0) == 0x0000);
    HK_CHECK(hk_rgb_scaled(255, 255, 255, 255) == 0xFFFF);
    /* Dimming makes it darker without changing the hue to something else. */
    HK_CHECK(hk_rgb_scaled(255, 0, 0, 128) < hk_rgb(255, 0, 0));

    /* ===== fill covers everything and nothing more ===== */
    reset();
    hk_draw_fill(fb, C);
    HK_CHECK_EQ_INT((int)painted(), (int)PIXELS);
    HK_CHECK(guards_intact());

    /* ===== a disc centred on the panel ===== */
    reset();
    hk_draw_disc(fb, 120, 120, 50, C);
    HK_CHECK(at(120, 120) == C);
    HK_CHECK(at(120, 71) == C);       /* just inside the top edge */
    HK_CHECK(at(120, 69) == 0);       /* just outside */
    HK_CHECK(at(0, 0) == 0);
    HK_CHECK(guards_intact());

    /* ===== every primitive clips, and that is the point ===== */
    reset();
    hk_draw_disc(fb, 0, 0, 200, C);            /* mostly off the top-left */
    HK_CHECK(guards_intact());
    hk_draw_disc(fb, 239, 239, 300, C);
    HK_CHECK(guards_intact());
    hk_draw_rect(fb, -50, -50, 500, 500, C);
    HK_CHECK(guards_intact());
    hk_draw_rect(fb, 230, 230, 100, 100, C);
    HK_CHECK(guards_intact());
    hk_draw_ring(fb, 120, 120, 400, 380, C);
    HK_CHECK(guards_intact());
    hk_draw_arc(fb, 120, 120, 500, 400, 900, C);
    HK_CHECK(guards_intact());
    hk_draw_text(fb, -80, -40, 30, 50, 5, "8888", C);
    HK_CHECK(guards_intact());
    hk_draw_text(fb, 220, 220, 30, 50, 5, "8888", C);
    HK_CHECK(guards_intact());

    /* ===== an inner radius past the outer is an empty ring, not a full plane ===== */
    reset();
    hk_draw_ring(fb, 120, 120, 40, 60, C);
    HK_CHECK_EQ_INT((int)painted(), 0);
    hk_draw_ring(fb, 120, 120, 40, 40, C);
    HK_CHECK_EQ_INT((int)painted(), 0);
    HK_CHECK(guards_intact());

    /* ===== a ring is hollow ===== */
    reset();
    hk_draw_ring(fb, 120, 120, 60, 40, C);
    HK_CHECK(at(120, 120) == 0);      /* the hole */
    HK_CHECK(at(120, 70) == C);       /* inside the band */
    HK_CHECK(at(120, 55) == 0);       /* outside it */
    HK_CHECK(guards_intact());

    /* ===== arcs start at twelve o'clock and run clockwise ===== */
    reset();
    hk_draw_arc(fb, 120, 120, 60, 50, 250, C);   /* a quarter turn */
    HK_CHECK(at(120, 65) == C);        /* twelve o'clock: drawn */
    HK_CHECK(at(175, 120) == C);       /* three o'clock: the end of the sweep */
    HK_CHECK(at(120, 175) == 0);       /* six o'clock: beyond it */
    HK_CHECK(at(65, 120) == 0);        /* nine o'clock: beyond it */
    HK_CHECK(guards_intact());

    /* A full sweep is a whole ring. */
    reset();
    hk_draw_arc(fb, 120, 120, 60, 50, 1000, C);
    const size_t full = painted();
    reset();
    hk_draw_ring(fb, 120, 120, 60, 50, C);
    HK_CHECK(full == painted());

    /* Nothing, and more than everything, both behave. */
    reset();
    hk_draw_arc(fb, 120, 120, 60, 50, 0, C);
    HK_CHECK_EQ_INT((int)painted(), 0);
    hk_draw_arc(fb, 120, 120, 60, 50, 5000, C);
    HK_CHECK(painted() == full);
    HK_CHECK(guards_intact());

    /* ===== glyphs ===== */
    reset();
    HK_CHECK(hk_draw_glyph(fb, 20, 20, 30, 50, 5, '8', C) == 40);
    const size_t eight = painted();
    reset();
    hk_draw_glyph(fb, 20, 20, 30, 50, 5, '1', C);
    const size_t one = painted();
    HK_CHECK(one > 0 && one < eight);          /* '1' is two segments of seven */

    /* An unknown character draws NOTHING rather than something wrong: a screen
     * that shows the wrong PIN is worse than one that shows a gap. */
    reset();
    HK_CHECK(hk_draw_glyph(fb, 20, 20, 30, 50, 5, '@', C) == 40);
    HK_CHECK_EQ_INT((int)painted(), 0);
    HK_CHECK(hk_draw_glyph(fb, 20, 20, 30, 50, 5, ' ', C) == 40);
    HK_CHECK_EQ_INT((int)painted(), 0);

    /* Hex, because the device id is hex. */
    for (const char *c = "0123456789ABCDEF"; *c; c++) {
        reset();
        hk_draw_glyph(fb, 20, 20, 30, 50, 5, *c, C);
        HK_CHECK(painted() > 0);
    }
    /* Lower case reads the same as upper. */
    reset();
    hk_draw_glyph(fb, 20, 20, 30, 50, 5, 'c', C);
    const size_t lower = painted();
    reset();
    hk_draw_glyph(fb, 20, 20, 30, 50, 5, 'C', C);
    HK_CHECK(lower == painted());

    /* ===== layout maths matches what drawing actually advances ===== */
    reset();
    const int advanced = hk_draw_text(fb, 10, 10, 24, 40, 4, "932C", C);
    HK_CHECK_EQ_INT(advanced, hk_draw_text_width(24, 4, 4));
    HK_CHECK(hk_draw_text_width(24, 4, 0) == 0);
    HK_CHECK(hk_draw_text(fb, 10, 10, 24, 40, 4, NULL, C) == 0);
    HK_CHECK(guards_intact());

    /* ===== a NULL buffer measures but never writes ===== */
    HK_CHECK(hk_draw_glyph(NULL, 0, 0, 24, 40, 4, '8', C) == 32);
    hk_draw_fill(NULL, C);
    hk_draw_disc(NULL, 10, 10, 5, C);
    hk_draw_ring(NULL, 10, 10, 9, 5, C);
    hk_draw_arc(NULL, 10, 10, 9, 5, 500, C);
    hk_draw_rect(NULL, 0, 0, 5, 5, C);
    HK_CHECK(guards_intact());

    /* ===== degenerate geometry draws nothing ===== */
    reset();
    hk_draw_disc(fb, 120, 120, 0, C);
    hk_draw_disc(fb, 120, 120, -5, C);
    hk_draw_rect(fb, 10, 10, 0, 10, C);
    hk_draw_rect(fb, 10, 10, 10, -3, C);
    hk_draw_glyph(fb, 10, 10, 0, 40, 4, '8', C);
    hk_draw_glyph(fb, 10, 10, 24, 40, 0, '8', C);
    HK_CHECK_EQ_INT((int)painted(), 0);
    HK_CHECK(guards_intact());
}
