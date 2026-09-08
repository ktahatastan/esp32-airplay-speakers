#include "hk_display.h"

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
#include "hk_identity.h"
#include "hk_pins.h"
#include "hk_storage.h"
#include "hk_ui.h"

static const char *TAG = "hk_lcd";

/* SPI2's IO_MUX pads are already spent on other functions, so its speed
 * advantage is unreachable here; SPI3 has no IO_MUX pads to lose. ADR-0017. */
#define HK_LCD_HOST SPI3_HOST

/* 40 MHz to start. The part will take more and esp_lcd routes through the GPIO
 * matrix, which on the S3 behaves like IO_MUX up to 80 MHz -- but the first
 * number to trust is one measured on this wiring, not one taken from a
 * datasheet, and flying leads are the least favourable case. */
#define HK_LCD_CLOCK_HZ (40 * 1000 * 1000)

/* Lowest of anything that draws. Audio and the network both pre-empt it, and
 * ADR-0007's one-millisecond synchronisation budget is the reason. */
#define HK_LCD_TASK_PRIO  1
#define HK_LCD_TASK_STACK 4096
#define HK_LCD_FRAME_MS   42          /* ~24 fps */
#define HK_LCD_STRIP_ROWS 40          /* rows per SPI transfer */

static esp_lcd_panel_handle_t s_panel;
static uint16_t              *s_frame;
static hk_identity_t          s_identity;
static char                   s_pin[HK_DISPLAY_PIN_MAX + 1];
static bool                   s_have_pin;
static TaskHandle_t           s_task;

/**
 * Load the setup PIN so the screen can show it.
 *
 * This is the whole reason ADR-0015 stores the key as itself: a WPA2 network
 * needs a passphrase the owner can type, and reading it off the speaker is what
 * removes the friction that decision otherwise imposed. It is shown only while
 * the setup window is open, and it is never logged.
 */
static void load_pin(void)
{
    size_t length = HK_DISPLAY_PIN_MAX;
    s_have_pin = false;
    memset(s_pin, 0, sizeof(s_pin));
    if (hk_storage_factory_get_blob("ap_pass", s_pin, &length) != ESP_OK) {
        return;
    }
    s_pin[length] = '\0';
    s_have_pin = (length > 0 && strlen(s_pin) == length);
}

/** Centre a string of `count` glyphs of this geometry. */
static int centred_x(int glyph_w, int thickness, int count)
{
    return (HK_DRAW_WIDTH - hk_draw_text_width(glyph_w, thickness, count)) / 2;
}

/**
 * One frame, from state alone.
 *
 * Deliberately a pure function of (state, phase): given the same inputs it
 * draws the same pixels, which is what makes the screen impossible to leave
 * stuck. `phase` only drives the animation the state already asked for.
 */
static void render(const hk_led_inputs_t *inputs, uint32_t phase_ms)
{
    const hk_led_state_t state = hk_led_resolve(inputs);
    const hk_led_pattern_t *pattern = hk_led_pattern(state);

    /* The same brightness curve the LED uses, so the two read as one device. */
    uint8_t level = (uint8_t)((unsigned)pattern->brightness * 255u / 100u);
    if (pattern->animation == HK_LED_ANIM_BREATHE && pattern->period_ms > 0) {
        const uint32_t p = phase_ms % pattern->period_ms;
        const uint32_t half = pattern->period_ms / 2;
        const uint32_t up = (p < half) ? p : (pattern->period_ms - p);
        const uint32_t span = 255u - HK_LED_BREATHE_FLOOR;
        const uint32_t lit = HK_LED_BREATHE_FLOOR + (span * up) / (half ? half : 1);
        level = (uint8_t)((unsigned)level * lit / 255u);
    } else if ((pattern->animation == HK_LED_ANIM_BLINK_SLOW ||
                pattern->animation == HK_LED_ANIM_BLINK_FAST) &&
               pattern->period_ms > 0) {
        if ((phase_ms % pattern->period_ms) >= pattern->period_ms / 2) {
            level = 0;
        }
    }

    const uint16_t ink = hk_rgb_scaled(pattern->red, pattern->green, pattern->blue, level);
    const uint16_t dim = hk_rgb_scaled(pattern->red, pattern->green, pattern->blue,
                                       (uint8_t)(level / 6));

    hk_draw_fill(s_frame, hk_rgb(4, 5, 12));
    /* Backgrounds may reach the edge; content may not. The corners of this
     * buffer are never visible on a round panel. */
    hk_draw_disc(s_frame, 120, 120, 118, hk_rgb(7, 9, 20));
    hk_draw_ring(s_frame, 120, 120, 116, 110, dim);
    hk_draw_ring(s_frame, 120, 120, 116, 110, ink);

    /* The device id, always: four hex characters is what tells four speakers
     * apart, and it is the one thing worth reading from across a room. */
    const int gw = 26, gh = 44, th = 5;
    hk_draw_text(s_frame, centred_x(gw, th, 4), 78, gw, gh, th, s_identity.suffix, ink);

    /* The setup PIN, only while the window is open. */
    if (inputs->provisioning && s_have_pin) {
        const int pw = 16, ph = 26, pt = 3;
        const int count = (int)strlen(s_pin);
        hk_draw_text(s_frame, centred_x(pw, pt, count), 140, pw, ph, pt, s_pin,
                     hk_rgb_scaled(247, 207, 134, 255));
    }
}

static void display_task(void *arg)
{
    (void)arg;
    const uint32_t start = (uint32_t)(esp_timer_get_time() / 1000);
    bool reported = false;

    for (;;) {
        /* Straight from the LED's own inputs. Not a copy kept in step by hand:
         * the screen renders the same struct the light does, so they cannot
         * disagree about what the device is doing. */
        hk_led_inputs_t snapshot;
        hk_ui_snapshot(&snapshot);

        const int64_t began = esp_timer_get_time();
        render(&snapshot, (uint32_t)(began / 1000) - start);
        const esp_err_t sent = esp_lcd_panel_draw_bitmap(s_panel, 0, 0,
                                                         HK_DRAW_WIDTH, HK_DRAW_HEIGHT,
                                                         s_frame);
        if (!reported) {
            /* One measurement, once: how long a full frame costs on this
             * wiring. G3 will want it, and a number nobody printed is a number
             * nobody has. */
            ESP_LOGI(TAG, "first frame %s in %lld us",
                     sent == ESP_OK ? "sent" : "FAILED",
                     esp_timer_get_time() - began);
            reported = true;
        }
        vTaskDelay(pdMS_TO_TICKS(HK_LCD_FRAME_MS));
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
    load_pin();

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
        .trans_queue_depth = 10,
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
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "invert");
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
                  "pin %s", HK_PIN_LCD_SCK, HK_PIN_LCD_MOSI, HK_PIN_LCD_CS,
             HK_PIN_LCD_DC, HK_PIN_LCD_RST, s_have_pin ? "loaded" : "absent");
    return ESP_OK;
}

bool hk_display_present(void)
{
    return s_panel != NULL && s_frame != NULL;
}
