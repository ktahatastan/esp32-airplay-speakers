/**
 * @file test_screen.c
 * @brief That the screen cannot arrive somewhere nobody designed.
 *
 * ADR-0017's claim is that the panel has no state of its own, so it cannot get
 * stuck. That claim rests on two properties, and neither is self-evident.
 *
 * The first is that hk_screen_choose() is TOTAL: every combination of inputs
 * resolves to a screen that exists. The loop below walks all of them -- there
 * are only a few hundred -- rather than sampling, because "none of the above"
 * is exactly the case a sample would miss.
 *
 * The second is that rendering any of them, with any content, stays inside the
 * framebuffer. The strings come off the network from whatever is streaming, so
 * this feeds the renderer the shapes an attacker or a careless source would:
 * strings filled to the last byte, invalid UTF-8, a title far wider than the
 * panel. The buffer is fenced with canaries on both sides.
 */
#include "hk_test.h"

#include <stdint.h>
#include <string.h>

#include "hk_draw.h"
#include "hk_screen.h"
#include "hk_sky.h"

#define GUARD  256
#define CANARY 0x5AA5u

static uint16_t fenced[GUARD + HK_DRAW_WIDTH * HK_DRAW_HEIGHT + GUARD];

static uint16_t *fence_reset(void)
{
    for (size_t i = 0; i < sizeof(fenced) / sizeof(fenced[0]); i++) {
        fenced[i] = CANARY;
    }
    return &fenced[GUARD];
}

static bool fence_intact(void)
{
    const size_t total = sizeof(fenced) / sizeof(fenced[0]);
    for (int i = 0; i < GUARD; i++) {
        if (fenced[i] != CANARY || fenced[total - 1 - (size_t)i] != CANARY) {
            return false;
        }
    }
    return true;
}

/** Fill a fixed buffer to its very last byte, terminator included. */
static void fill_max(char *dst, size_t size, char c)
{
    memset(dst, c, size - 1);
    dst[size - 1] = '\0';
}

void test_screen(void)
{
    HK_CHECK(hk_sky_init(0x484B01u));

    /* --- choose() is total ------------------------------------------ */
    hk_view_t v;
    int seen[HK_SCREEN_COUNT] = {0};
    /* Twelve independent bits, not eight reused ones. The first version of this
     * loop drove audio_locked from the same bit as `ready` and `charging` from
     * the same bit as `battery_low`, which made three screens unreachable and
     * looked exactly like the product bug this test exists to find. Inputs that
     * are independent in the struct have to be independent here.
     *
     * It caught a second one the same way: HK_SCREEN_CONFIRM_SETUP was added to
     * the enum and to hk_screen_choose(), but confirm_setup was not added to
     * this loop -- so the completeness check reported an unreachable screen,
     * which is exactly what it is for. */
    for (unsigned bits = 0; bits < 8192u; bits++) {
        for (int hold = HK_BUTTON_HOLD_NONE; hold <= HK_BUTTON_HOLD_FACTORY_ARMED; hold++) {
            memset(&v, 0, sizeof(v));
            v.led.error        = (bits & (1u << 0)) != 0u;
            v.led.ota          = (bits & (1u << 1)) != 0u;
            v.led.battery_low  = (bits & (1u << 2)) != 0u;
            v.led.playing      = (bits & (1u << 3)) != 0u;
            v.led.ready        = (bits & (1u << 4)) != 0u;
            v.led.connecting   = (bits & (1u << 5)) != 0u;
            v.led.provisioning = (bits & (1u << 6)) != 0u;
            v.led.booting      = (bits & (1u << 7)) != 0u;
            v.led.button_hold  = (hk_button_hold_t)hold;
            v.audio_locked     = (bits & (1u << 8)) != 0u;
            v.charging         = (bits & (1u << 9)) != 0u;
            v.have_battery     = (bits & (1u << 10)) != 0u;
            v.have_ble_qr      = (bits & (1u << 11)) != 0u;
            v.confirm_setup    = (bits & (1u << 12)) != 0u;

            const hk_screen_id_t id = hk_screen_choose(&v);
            HK_CHECK(id >= 0 && id < HK_SCREEN_COUNT);
            if (id >= 0 && id < HK_SCREEN_COUNT) {
                seen[id]++;
            }
            HK_CHECK(strcmp(hk_screen_name(id), "unknown") != 0);
        }
    }
    /* Not a coverage metric: a screen no input can reach is a screen that was
     * designed and then orphaned, and it would never be seen to be wrong. */
    for (int i = 0; i < HK_SCREEN_COUNT; i++) {
        HK_CHECK(seen[i] > 0);
    }
    HK_CHECK_EQ_STR(hk_screen_name(HK_SCREEN_COUNT), "unknown");

    /* A held button outranks everything, because the owner is holding it now
     * and needs to know what letting go will do. */
    memset(&v, 0, sizeof(v));
    v.led.error = true;
    v.led.playing = true;
    v.led.button_hold = HK_BUTTON_HOLD_FACTORY_ARMED;
    HK_CHECK_EQ_INT(hk_screen_choose(&v), HK_SCREEN_HOLD_FACTORY);
    v.led.button_hold = HK_BUTTON_HOLD_NETWORK_ARMED;
    HK_CHECK_EQ_INT(hk_screen_choose(&v), HK_SCREEN_HOLD_NETWORK);

    /* Ready with audio refused is its own screen, not the idle one. */
    memset(&v, 0, sizeof(v));
    v.led.ready = true;
    HK_CHECK_EQ_INT(hk_screen_choose(&v), HK_SCREEN_READY);
    v.audio_locked = true;
    HK_CHECK_EQ_INT(hk_screen_choose(&v), HK_SCREEN_AUDIO_LOCKED);

    /* Charging is only a screen once something has measured the pack. A
     * percentage nobody measured must not reach the panel. */
    memset(&v, 0, sizeof(v));
    v.charging = true;
    HK_CHECK(hk_screen_choose(&v) != HK_SCREEN_CHARGING);
    v.have_battery = true;
    HK_CHECK_EQ_INT(hk_screen_choose(&v), HK_SCREEN_CHARGING);

    /* --- rendering stays inside the buffer -------------------------- */
    for (int id = 0; id < HK_SCREEN_COUNT; id++) {
        uint16_t *buf = fence_reset();

        memset(&v, 0, sizeof(v));
        fill_max(v.name, sizeof(v.name), 'W');
        fill_max(v.suffix, sizeof(v.suffix), 'F');
        fill_max(v.pin, sizeof(v.pin), '8');
        fill_max(v.ble_qr, sizeof(v.ble_qr), 'Q');
        fill_max(v.wifi_qr, sizeof(v.wifi_qr), 'Z');
        fill_max(v.media.title, sizeof(v.media.title), 'M');
        fill_max(v.media.artist, sizeof(v.media.artist), 'A');
        v.have_pin = v.have_ble_qr = v.have_wifi_qr = true;
        v.have_metadata = true;
        v.have_rssi = true;   v.rssi_dbm = -30;
        v.have_battery = true; v.battery_percent = 999;
        v.have_current = true; v.current_ma = -32000;
        v.have_pack_mv = true; v.pack_mv = 99999;
        v.have_temperature = true; v.temperature_c = -273;
        v.media.duration_secs = 1u;
        v.media.position_secs = 4000000000u;   /* a stale report from the future */
        v.volume_percent = 100;

        for (uint32_t t = 0; t < 3000u; t += 617u) {
            hk_screen_render(buf, (hk_screen_id_t)id, &v, t, t / 2u, 255);
            hk_screen_volume_overlay(buf, &v, t % HK_SCREEN_VOLUME_MS);
        }
        HK_CHECK(fence_intact());
    }

    /* Invalid UTF-8 in a track title must terminate and stay in bounds. A
     * title is the least trustworthy string on this device: it comes from
     * whatever is streaming, and a truncated multi-byte sequence at the end of
     * a 64-byte field is the ordinary case, not a contrived one. */
    {
        uint16_t *buf = fence_reset();
        memset(&v, 0, sizeof(v));
        v.have_metadata = true;
        v.led.playing = true;
        static const char broken[] = {
            (char)0xC3, (char)0x28,            /* bad continuation      */
            (char)0xE2, (char)0x82,            /* truncated three-byte  */
            (char)0xF0, (char)0x9F, (char)0x92,/* truncated four-byte   */
            (char)0x80, (char)0x80,            /* orphan continuations  */
            (char)0xFF, (char)0xFE,            /* never valid at all    */
            'o', 'k', '\0'
        };
        memcpy(v.media.title, broken, sizeof(broken));
        memcpy(v.media.artist, broken, sizeof(broken));
        /* And one that runs to the very last byte with no terminator room. */
        memset(v.media.album, (int)(unsigned char)0xF0, sizeof(v.media.album) - 1);
        v.media.album[sizeof(v.media.album) - 1] = '\0';
        for (uint32_t t = 0; t < 2000u; t += 311u) {
            hk_screen_render(buf, HK_SCREEN_PLAYING, &v, t, t, 255);
        }
        HK_CHECK(fence_intact());
    }

    /* The same inputs draw the same pixels. This is what "no state of its own"
     * means in practice: nothing accumulates between frames, so a screen cannot
     * be in a condition its inputs do not describe. */
    {
        static uint16_t first[HK_DRAW_WIDTH * HK_DRAW_HEIGHT];
        uint16_t *buf = fence_reset();
        memset(&v, 0, sizeof(v));
        strcpy(v.name, "Harman Kardom 932C");
        strcpy(v.suffix, "932C");
        v.led.ready = true;
        hk_screen_render(buf, HK_SCREEN_READY, &v, 7000u, 1200u, 255);
        memcpy(first, buf, sizeof(first));
        /* Draw something else in between, then ask for the first frame again. */
        hk_screen_render(buf, HK_SCREEN_PLAYING, &v, 9000u, 400u, 255);
        hk_screen_render(buf, HK_SCREEN_READY, &v, 7000u, 1200u, 255);
        HK_CHECK(memcmp(first, buf, sizeof(first)) == 0);
        HK_CHECK(fence_intact());
    }
}
