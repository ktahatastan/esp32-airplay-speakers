#include <stddef.h>

#include "hk_icons.h"

#include "hk_gfx.h"

/*
 * The table lives in hk_icons_data.c, which tools/gen_icons.py writes. It is
 * declared here rather than in hk_icons.h because nothing outside this file has
 * any business indexing it -- callers go through hk_icon(), which is the only
 * path that bounds-checks.
 *
 * One entry needs saying out loud, because the header cannot: HK_ICON_BATTERY
 * is an empty shell. A stored mask is one state of charge, so the case and its
 * terminal are all that is here, and the hole is left for a caller that wants
 * to show a level to fill -- 14 x 6 pixels at (2, 2) relative to where the icon
 * was placed, which clears the case wall on every side. No screen fills it
 * today; the charging screen shows a number instead.
 */
extern const hk_icon_t hk_icons_generated[HK_ICON_COUNT];

/*
 * The answer to an id nobody defined.
 *
 * Returning NULL would push the check onto every call site, and the one call
 * site that forgot would fault at a point in the render loop with no clue as to
 * which screen asked for which icon. A zero-size icon draws nothing, which on a
 * panel looks like a missing icon and is exactly as informative.
 */
static const hk_icon_t hk_icon_none = { "none", 0, 0, NULL };

const hk_icon_t *hk_icon(hk_icon_id_t id)
{
    /* Unsigned compare, so a negative id folds into the same branch as a large
     * one instead of indexing behind the table. */
    if ((unsigned)id >= (unsigned)HK_ICON_COUNT) {
        return &hk_icon_none;
    }
    return &hk_icons_generated[id];
}

void hk_icon_draw(uint16_t *buf, hk_icon_id_t id, int x, int y,
                  uint16_t colour, uint8_t alpha)
{
    const hk_icon_t *ic = hk_icon(id);

    if (buf == NULL || ic->coverage == NULL || ic->width == 0 || ic->height == 0) {
        return;
    }
    /* The generator pads each row to whole bytes, so the stride follows from
     * the width and is not worth a field in the table. */
    hk_gfx_blit_a4(buf, x, y, ic->width, ic->height, ic->coverage,
                   (ic->width + 1) / 2, colour, alpha);
}

/*
 * The signal meter.
 *
 * Four bars two pixels wide on a three pixel pitch, 11 x 8 in total, rising
 * 2-4-6-8 from a shared baseline. Every edge lands on a pixel boundary, which
 * is the point: a 2 px bar placed at a fractional x is two 50% columns, and
 * four of those in a row read as a grey smear rather than as a meter. This is
 * the one place in the set where NOT antialiasing is the better picture.
 *
 * The design's own meter (viewBox 0 0 16 12, drawn at 11 x 8) has a 4.4-unit
 * pitch and rounded ends; at this size the pitch is 3.03 px and the rounding is
 * a tenth of a pixel, so both are snapped rather than approximated.
 */
#define BARS_COUNT  4
#define BAR_WIDTH   2
#define BAR_PITCH   3
#define BARS_HEIGHT 8
#define BARS_WIDTH  ((BARS_COUNT - 1) * BAR_PITCH + BAR_WIDTH)

void hk_icon_wifi_bars(uint16_t *buf, int x, int y, int bars,
                       uint16_t colour, uint8_t alpha)
{
    if (buf == NULL) {
        return;
    }
    if (bars < 0) {
        bars = 0;
    }
    if (bars > BARS_COUNT) {
        bars = BARS_COUNT;
    }

    /*
     * The meter as a whole, tested wide, before any of the per-bar arithmetic
     * runs. hk_gfx_rect() clips each bar on its own, but only after this
     * function has already worked out x + 9 and y + 8 - h in int -- and for a
     * coordinate near the end of the range those additions overflow rather
     * than merely land off screen, which is undefined and which the optimiser
     * is entitled to assume never happens. hk_gfx_blit_a4() widens its own
     * offsets for the same reason; this is the one place in the icon layer
     * that does its own placement and so has to repeat the guard.
     */
    if ((long long)x + BARS_WIDTH <= 0 || x >= HK_DRAW_WIDTH ||
        (long long)y + BARS_HEIGHT <= 0 || y >= HK_DRAW_HEIGHT) {
        return;
    }

    /*
     * The unlit bars at 72/255 of the caller's alpha -- the 0.28 opacity the
     * design uses for them. They have to stay visible: a meter that hides its
     * empty bars changes width as the signal moves, and a chip that changes
     * width drags the row next to it around.
     */
    const uint8_t dim = (uint8_t)(((unsigned)alpha * 72u) / 255u);

    for (int i = 0; i < BARS_COUNT; i++) {
        const int h = 2 * (i + 1);
        hk_gfx_rect(buf, x + i * BAR_PITCH, y + BARS_HEIGHT - h, BAR_WIDTH, h,
                    colour, (i < bars) ? alpha : dim);
    }
}

int hk_icon_bars_from_rssi(int rssi_dbm)
{
    /*
     * The two ends are fixed by the contract: -90 dBm and below is nothing,
     * above -55 is everything. -90 is roughly where an 802.11n receiver runs
     * out of sensitivity even at the lowest rate, and -55 is where more signal
     * stops buying anything, so outside that span the meter has nothing left to
     * say.
     *
     * Of the three boundaries inside it, only -67 is chosen rather than spaced:
     * it is the level Wi-Fi site surveys use as the floor for voice and video,
     * which for this product is the floor for AirPlay not stuttering. So three
     * bars means "this will stream" and two means "it might not", which is the
     * only distinction in this widget a user can act on. -78 then splits what
     * is left, giving steps of 12, 11 and 12 dB.
     *
     * Equal steps in dB are not equal steps in quality -- the scale is
     * logarithmic and the throughput curve is not. That is accepted: a four-bar
     * meter is asked to be monotone and stable, not calibrated, and a bar that
     * jitters between two values while the speaker sits still is the failure
     * that matters here.
     */
    if (rssi_dbm >= -55) {
        return 4;
    }
    if (rssi_dbm >= -67) {
        return 3;
    }
    if (rssi_dbm >= -78) {
        return 2;
    }
    if (rssi_dbm >= -90) {
        return 1;
    }
    return 0;
}
