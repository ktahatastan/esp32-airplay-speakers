#include "hk_display.h"

#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_draw.h"
#include "hk_gfx.h"
#include "hk_identity.h"
#include "hk_palette.h"
#include "hk_pins.h"
#include "hk_font.h"
#include "hk_screen.h"
#include "hk_sky.h"
#include "hk_storage.h"
#include "hk_ui.h"
#include "hk_view.h"

static const char *TAG = "hk_lcd";

/* SPI2's IO_MUX pads are already spent on other functions, so its speed
 * advantage is unreachable here; SPI3 has no IO_MUX pads to lose. ADR-0017. */
#define HK_LCD_HOST SPI3_HOST

/* 80 MHz, and the frame budget is why.
 *
 * One full frame is 240*240*2 = 115,200 bytes. At 40 MHz that transfer alone is
 * 23 ms of a 42 ms frame, which leaves the galaxy and the foreground about as
 * much time as they need and no margin at all. At 80 MHz it is 11.5 ms. The S3
 * routes SPI3 through the GPIO matrix, which the datasheet says behaves like
 * IO_MUX up to 80 MHz, so this is the ceiling rather than a guess -- but it is
 * a ceiling on flying leads, and the measured per-frame time logged below is
 * what decides whether it holds. If frames start failing, this is the first
 * number to halve. */
#define HK_LCD_CLOCK_HZ (80 * 1000 * 1000)

/* Lowest of anything that draws. Audio and the network both pre-empt it, and
 * ADR-0007's one-millisecond synchronisation budget is the reason. */
#define HK_LCD_TASK_PRIO  1
/* Eight, not four.
 *
 * Three things share this stack and the largest of them is intermittent: the
 * view snapshot the task copies is about 900 bytes, and hk_qr_encode() needs
 * roughly 1.4 KB more on this part -- but only on the frame where a pairing
 * payload changes, which is exactly the kind of peak that a stack sized for
 * the common frame survives in testing and overflows in a living room. */
#define HK_LCD_TASK_STACK 8192
/* 30 fps, because the measurement now allows it.
 *
 * This was 42 ms (~24 fps) while a frame cost 112 ms and the number was
 * aspirational rather than a budget. A frame now costs 22 ms measured on the
 * board -- 9.5 ms of that is the panel transfer and cannot shrink -- so 33 ms
 * leaves real headroom and the extra six frames a second are exactly where a
 * transition stops looking like steps. */
#define HK_LCD_FRAME_MS   33          /* ~30 fps; measured cost is 22 ms */
#define HK_LCD_STRIP_ROWS 40          /* rows per SPI transfer */

/* How long both setup codes stay up before swapping. Both transports are
 * open at once (ADR-0016) and the owner picks; the screen offers each in
 * turn rather than making them choose from a menu. Long enough to scan. */
#define HK_LCD_PAIR_SWAP_MS 6000u

static esp_lcd_panel_handle_t s_panel;
static uint16_t              *s_frame;
static hk_identity_t          s_identity;
static TaskHandle_t           s_task;

/**
 * What is on screen, and how it got there.
 *
 * This is the ONLY state the display layer keeps, and it is all derived: the
 * screen id comes from hk_screen_choose() every frame, and the timestamps exist
 * so an entry animation can start from zero. Nothing here can be left stale,
 * because nothing here is authoritative -- if this struct were wrong, the next
 * frame would correct it.
 */
static struct {
    hk_screen_id_t current;
    hk_screen_id_t previous;
    uint32_t       entered_ms;
    uint32_t       previous_entered_ms;
    uint32_t       transition_began_ms;   /**< 0 when settled */
} s_scene;

/**
 * Which screen to be on, including the one rule the state machine cannot hold.
 *
 * Both provisioning transports are open at the same time (ADR-0016) and the
 * owner chooses; hk_screen_choose() cannot express "offer each in turn" because
 * it is a pure function of the view and has no clock. So the cadence lives
 * here, where the clock already is, and stays out of the decision itself.
 */
static hk_screen_id_t wanted_screen(const hk_view_t *view, uint32_t now_ms)
{
    hk_screen_id_t id = hk_screen_choose(view);
    if (id == HK_SCREEN_PAIR_BLE && (view->have_wifi_qr || view->have_pin)) {
        if (((now_ms / HK_LCD_PAIR_SWAP_MS) & 1u) != 0u) {
            id = HK_SCREEN_PAIR_AP;
        }
    }
    return id;
}

/**
 * Collapse everything outside a radius to the shell colour.
 *
 * This is the iris, and it lives here rather than in hk_gfx because of a
 * measurement. hk_gfx_iris() visits all 57,600 pixels and blends each one,
 * which means reading the framebuffer back out of PSRAM; on the board that cost
 * about 50 ms, and it was the whole reason transitions did not look smooth.
 * Nearly all of that work is wasted: inside the circle the answer is "leave it
 * alone", and outside it the answer is a constant that needs no read at all.
 *
 * So each row is a span. The interior is skipped, the exterior is written
 * without being read, and only the two boundary pixels per row are blended --
 * a few hundred blends instead of tens of thousands, for an edge that would
 * otherwise step visibly as the radius animates.
 */
static void collapse_outside(uint16_t *buf, int radius)
{
    if (radius >= 170) {                 /* past the corners: nothing to do */
        return;
    }
    const int r2 = radius * radius;
    for (int y = 0; y < HK_DRAW_HEIGHT; y++) {
        const int dy = y - 120;
        const int inside2 = r2 - dy * dy;
        int half = 0;
        if (inside2 > 0) {
            int lo = 0, hi = radius;     /* integer sqrt: the row's half-width */
            while (lo < hi) {
                const int mid = (lo + hi + 1) / 2;
                if (mid * mid <= inside2) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            half = lo;
        }
        uint16_t *row = buf + (size_t)y * HK_DRAW_WIDTH;
        int x0 = 120 - half;
        int x1 = 120 + half;
        if (x0 < 0) {
            x0 = 0;
        }
        if (x1 > HK_DRAW_WIDTH) {
            x1 = HK_DRAW_WIDTH;
        }
        for (int x = 0; x < x0; x++) {
            row[x] = HK_C_SHELL;
        }
        for (int x = x1; x < HK_DRAW_WIDTH; x++) {
            row[x] = HK_C_SHELL;
        }
        if (x0 > 0 && x0 < HK_DRAW_WIDTH) {
            row[x0] = hk_gfx_blend(row[x0], HK_C_SHELL, 128);
        }
        if (x1 > 1 && x1 <= HK_DRAW_WIDTH) {
            row[x1 - 1] = hk_gfx_blend(row[x1 - 1], HK_C_SHELL, 128);
        }
    }
}

/**
 * One frame: the screen, the transition it may be in, and the volume overlay.
 *
 * The transition is an iris rather than a cross-fade. Two lit screens mixed
 * together pass through a brighter middle than either of them, which reads as a
 * flash on a panel this small; collapsing one and expanding the other never
 * shows both at once, so there is no middle to be wrong.
 */
static void compose(const hk_view_t *view, uint32_t now_ms)
{
    const uint32_t half = HK_SCREEN_TRANSITION_MS / 2u;

    if (s_scene.transition_began_ms != 0u) {
        const uint32_t age = now_ms - s_scene.transition_began_ms;
        if (age < half) {
            hk_screen_render(s_frame, s_scene.previous, view, now_ms,
                             s_scene.previous_entered_ms, 255);
            collapse_outside(s_frame, 170 - (int)((170u * age) / half));
        } else if (age < HK_SCREEN_TRANSITION_MS) {
            hk_screen_render(s_frame, s_scene.current, view, now_ms,
                             s_scene.entered_ms, 255);
            collapse_outside(s_frame, (int)((170u * (age - half)) / half));
        } else {
            s_scene.transition_began_ms = 0u;
            hk_screen_render(s_frame, s_scene.current, view, now_ms,
                             s_scene.entered_ms, 255);
        }
    } else {
        hk_screen_render(s_frame, s_scene.current, view, now_ms, s_scene.entered_ms, 255);
    }

    /* Over the top of whatever that was, including a transition: volume changes
     * while something else is happening, and the owner turned the knob because
     * they were listening to the thing underneath. */
    if (view->volume_changed_ms != 0u && now_ms >= view->volume_changed_ms) {
        const uint32_t age = now_ms - view->volume_changed_ms;
        if (age < HK_SCREEN_VOLUME_MS) {
            hk_screen_volume_overlay(s_frame, view, age);
        }
    }
}

/**
 * Three seconds of something no other firmware would draw.
 *
 * "Is the panel wired?" and "is our image reaching it?" are different questions
 * and a status screen answers neither: a dark panel and a panel showing
 * somebody else's demo look the same from here, because 4-wire SPI has no
 * readback. Four flat colours in a fixed order and a shape nothing else draws
 * settle it from across the room.
 */
static void self_test(void)
{
    static const struct { uint8_t r, g, b; const char *name; } steps[] = {
        {255,   0,   0, "red"},
        {  0, 255,   0, "green"},
        {  0,   0, 255, "blue"},
        {255, 255, 255, "white"},
    };
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        hk_draw_fill(s_frame, hk_rgb(steps[i].r, steps[i].g, steps[i].b));
        /* A cross, so a rotated or offset panel shows it off-centre rather than
         * looking correct by accident. */
        hk_draw_rect(s_frame, 0, 116, HK_DRAW_WIDTH, 8, hk_rgb(0, 0, 0));
        hk_draw_rect(s_frame, 116, 0, 8, HK_DRAW_HEIGHT, hk_rgb(0, 0, 0));
        ESP_LOGI(TAG, "self test: %s", steps[i].name);
        (void)esp_lcd_panel_draw_bitmap(s_panel, 0, 0, HK_DRAW_WIDTH, HK_DRAW_HEIGHT, s_frame);
        vTaskDelay(pdMS_TO_TICKS(450));
    }

    /* The orientation card.
     *
     * Four flat colours prove our pixels arrive; they say nothing about which
     * way round they arrive, because a centred cross looks identical under
     * every flip. This card cannot. It names each edge and sets the names in
     * letters that are asymmetric in both axes, so a mirrored panel shows the
     * words backwards and a flipped one shows them on the wrong sides. That
     * separates the two axes, which is the thing neither the cross nor a
     * photograph of a status screen could do -- and it is why the first attempt
     * at this fix was a guess. */
    hk_gfx_clip_reset();
    hk_draw_fill(s_frame, hk_rgb(6, 7, 16));
    hk_gfx_disc(s_frame, HK_Q4(120), HK_Q4(120), HK_Q4(118), HK_C_VOID, 255);
    hk_font_draw(s_frame, hk_font_body(),
                 120 - hk_font_measure(hk_font_body(), "UST") / 2, 46,
                 "UST", HK_C_GOOD, 255);
    hk_font_draw(s_frame, hk_font_body(), 24, 126, "SOL", HK_C_ACCENT, 255);
    hk_font_draw(s_frame, hk_font_body(),
                 216 - hk_font_measure(hk_font_body(), "SAG"), 126,
                 "SAG", HK_C_WAIT, 255);
    hk_font_draw(s_frame, hk_font_body(),
                 120 - hk_font_measure(hk_font_body(), "ALT") / 2, 206,
                 "ALT", HK_C_EMBER, 255);
    /* One large glyph with no symmetry at all, so "backwards" is unmistakable
     * without reading the words. */
    hk_draw_text(s_frame, 100, 96, 32, 52, 7, "F", HK_C_INK);
    ESP_LOGI(TAG, "orientation card: UST top, SOL left, SAG right, ALT bottom, "
                  "and an F that must not read backwards");
    (void)esp_lcd_panel_draw_bitmap(s_panel, 0, 0, HK_DRAW_WIDTH, HK_DRAW_HEIGHT, s_frame);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "self test done. If the panel showed red, green, blue and white "
                  "with a black cross, our pixels reach it.");
}

static void display_task(void *arg)
{
    (void)arg;
#if CONFIG_HK_DISPLAY_SELF_TEST
    self_test();
#else
    (void)self_test;
#endif
    const uint32_t start = (uint32_t)(esp_timer_get_time() / 1000);
    bool reported = false;
    uint32_t slow_frames = 0;
    uint32_t frames = 0;

    s_scene.current = HK_SCREEN_BOOT;
    s_scene.previous = HK_SCREEN_BOOT;

    for (;;) {
        hk_view_t view;
        hk_view_snapshot(&view);

        const int64_t began = esp_timer_get_time();
        const uint32_t now = (uint32_t)(began / 1000) - start;

        const hk_screen_id_t wanted = wanted_screen(&view, now);
        if (wanted != s_scene.current) {
            ESP_LOGI(TAG, "%s -> %s", hk_screen_name(s_scene.current),
                     hk_screen_name(wanted));
            s_scene.previous = s_scene.current;
            s_scene.previous_entered_ms = s_scene.entered_ms;
            s_scene.current = wanted;
            s_scene.entered_ms = now;
            s_scene.transition_began_ms = now;
        }

        compose(&view, now);
        const int64_t drawn = esp_timer_get_time();
        const esp_err_t sent = esp_lcd_panel_draw_bitmap(s_panel, 0, 0,
                                                         HK_DRAW_WIDTH, HK_DRAW_HEIGHT,
                                                         s_frame);
        const int64_t finished = esp_timer_get_time();

        frames++;
        if (!reported) {
            /* After the first frame, not before it. The earlier per-step
             * measurement was taken before any transfer and reported 2,272 B;
             * the SPI path allocates its DMA buffers lazily, so the real cost
             * only exists once something has actually been sent. */
            ESP_LOGI(TAG, "first frame %s: render %lld us, transfer %lld us, "
                          "sky %u us; internal free %u B (largest block %u B)",
                     sent == ESP_OK ? "sent" : "FAILED",
                     drawn - began, finished - drawn,
                     (unsigned)hk_sky_last_us(),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            reported = true;
        }

        /* A frame that misses its slot is not a crash and must not be treated
         * as one, but a screen that quietly runs at half speed is a screen
         * nobody knows is slow. Count them and say so periodically. */
        const uint32_t cost_ms = (uint32_t)((finished - began) / 1000);
        if (cost_ms > HK_LCD_FRAME_MS) {
            slow_frames++;
        }
        if ((frames % 120u) == 0u) {
            ESP_LOGI(TAG, "%u frames, %u over %u ms; last %u ms (sky %u us)",
                     (unsigned)frames, (unsigned)slow_frames,
                     (unsigned)HK_LCD_FRAME_MS, (unsigned)cost_ms,
                     (unsigned)hk_sky_last_us());
        }

        const uint32_t spent = cost_ms;
        vTaskDelay(pdMS_TO_TICKS(spent >= HK_LCD_FRAME_MS ? 1 : HK_LCD_FRAME_MS - spent));
    }
}

esp_err_t hk_display_start(void)
{
    if (s_panel != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK ||
        hk_identity_from_mac(mac, &s_identity) != HK_IDENTITY_OK) {
        /* Without an identity there is nothing worth putting on screen, and a
         * screen showing the wrong speaker's name is worse than a dark one. */
        ESP_LOGE(TAG, "no device identity; the panel stays dark");
        return ESP_ERR_INVALID_STATE;
    }
    /* The identity is the one thing the screen needs before its first frame.
     * Everything else -- the setup codes, the metadata, the signal -- is
     * published by whoever owns it, when it happens. The setup codes in
     * particular are hk_network's: it already holds the only copy of that
     * secret, and a second reader here would be a second place to forget to
     * wipe it. */
    hk_view_set_identity(s_identity.airplay, s_identity.suffix);

    const spi_bus_config_t bus = {
        .sclk_io_num = HK_PIN_LCD_SCK,
        .mosi_io_num = HK_PIN_LCD_MOSI,
        .miso_io_num = -1,               /* 4-wire SPI: nothing is read back */
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        /* A strip, not a frame.
         *
         * This was changed on a hypothesis that turned out to be wrong -- that
         * a frame-sized limit made the SPI driver reserve internal RAM to match
         * -- and the per-step measurement below disproved it: the whole display
         * path costs 2,272 B of internal RAM either way. It stays a strip
         * because bounding a single transfer is still the right shape, and
         * esp_lcd splits a draw_bitmap into transfers of at most this size
         * regardless. The apparent 131 KB drop was the AirPlay receiver's own
         * allocation landing before the probe rather than after it. */
        .max_transfer_sz = HK_DRAW_WIDTH * HK_LCD_STRIP_ROWS * (int)sizeof(uint16_t),
    };
    /* Internal RAM is the scarce kind and the display path spends a surprising
     * amount of it. Each step reports what it cost, because a total nobody can
     * attribute is a total nobody can reduce. */
    size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(HK_LCD_HOST, &bus, SPI_DMA_CH_AUTO),
                        TAG, "spi bus");
    size_t after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "internal cost: spi bus %d B", (int)(before - after));
    before = after;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = HK_PIN_LCD_CS,
        .dc_gpio_num = HK_PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = HK_LCD_CLOCK_HZ,
        /* Two, not ten. Each queued transfer of a PSRAM source needs its own
           DMA-capable buffer in INTERNAL memory, so the depth multiplies the
           strip size by the scarcest resource on the part. Two is enough to
           keep the bus busy while the next strip is prepared. */
        .trans_queue_depth = 2,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)HK_LCD_HOST,
                                                 &io_config, &io),
                        TAG, "panel io");
    after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "internal cost: panel io %d B", (int)(before - after));
    before = after;

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = HK_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &panel_config, &s_panel),
                        TAG, "gc9a01");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    /* INVOFF, not INVON.
     *
     * Most GC9A01 examples set this true, and the vendor init table in
     * managed_components/espressif__esp_lcd_gc9a01 issues neither command, so
     * whatever we pass here is what the panel gets. On this module true is
     * wrong, and the bench photograph settles it: every colour came back as its
     * exact complement. The near-black #04050C ground rendered as a pale field,
     * the provisioning blue (0,80,255) rendered orange -- (255,175,0) -- for
     * both the ring and the device id, and the amber PIN (247,207,134) rendered
     * as the deep blue-violet (8,48,121). Three independent colours, three
     * exact complements. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, false), TAG, "invert");
    /* Y only.
     *
     * This was (true, true) for a while, read off a photograph of the old
     * seven-segment status screen, and it was half wrong. That screen could not
     * settle the question: its glyphs are stroke forms, several of which look
     * plausible reversed, so "the id appeared below the PIN" was real evidence
     * about Y while "the glyphs read mirrored" was a guess about X.
     *
     * The orientation card in self_test() answered it properly -- named edges
     * plus a letter with no symmetry in either axis -- and the operator read it
     * back as horizontally mirrored with the vertical correct. So the panel's
     * scan order differs from the module's own top in Y alone, and X was never
     * wrong. The lesson belongs to the self test rather than here: four flat
     * colours and a centred cross prove that pixels arrive and say nothing
     * whatever about which way up they arrive. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, true), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on");
    after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "internal cost: panel %d B", (int)(before - after));
    before = after;

    /* The framebuffer lives in PSRAM: 115 200 bytes is most of what internal
     * RAM has left once both radios are up, and this board has 8 MB of the
     * other kind. It must still be DMA-capable, which on the S3 PSRAM is. */
    s_frame = heap_caps_malloc(HK_DRAW_WIDTH * HK_DRAW_HEIGHT * sizeof(uint16_t),
                               MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    if (s_frame == NULL) {
        ESP_LOGE(TAG, "no room for a %u byte framebuffer",
                 (unsigned)(HK_DRAW_WIDTH * HK_DRAW_HEIGHT * sizeof(uint16_t)));
        return ESP_ERR_NO_MEM;
    }

    after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "internal cost: framebuffer %d B (it lives in psram)",
             (int)(before - after));

    if (xTaskCreate(display_task, "hk_lcd", HK_LCD_TASK_STACK, NULL,
                    HK_LCD_TASK_PRIO, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "panel up on spi3: sck %d, mosi %d, cs %d, dc %d, rst %d; "
                  "%d MHz", HK_PIN_LCD_SCK, HK_PIN_LCD_MOSI, HK_PIN_LCD_CS,
             HK_PIN_LCD_DC, HK_PIN_LCD_RST, HK_LCD_CLOCK_HZ / 1000000);
    return ESP_OK;
}

bool hk_display_present(void)
{
    return s_panel != NULL && s_frame != NULL;
}
