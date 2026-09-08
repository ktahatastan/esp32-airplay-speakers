/**
 * @file hk_view.c
 * @brief The snapshot the screen renders from.
 *
 * One struct, one lock, one writer per field. The lock is a portMUX critical
 * section rather than a mutex to match hk_ui.c's existing convention and
 * because the alternative is worse here: the AirPlay receiver's RTSP task sets
 * metadata, the display task reads it, and a mutex between them would let the
 * display task block on a network task. The copy is under a kilobyte, so the
 * section is on the order of a microsecond -- far inside the I2S DMA's
 * tolerance, and the reason the copy is a plain struct assignment rather than
 * anything cleverer.
 */
#include "hk_view.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_ui.h"

static hk_view_t    s_view;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

/** Copy into a fixed buffer, always terminated, never reading past `src`. */
static void set_text(char *dst, size_t size, const char *src)
{
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    const size_t length = strnlen(src, size - 1);
    memcpy(dst, src, length);
    dst[length] = '\0';
}

void hk_view_snapshot(hk_view_t *out)
{
    if (out == NULL) {
        return;
    }
    /* The LED's own inputs come from hk_ui, which owns them; everything else
     * lives here. Taking them separately would let the two disagree by a frame,
     * so the LED half is copied inside the same section. */
    hk_led_inputs_t led;
    hk_ui_snapshot(&led);

    taskENTER_CRITICAL(&s_lock);
    *out = s_view;
    taskEXIT_CRITICAL(&s_lock);
    out->led = led;
}

void hk_view_set_identity(const char *name, const char *suffix)
{
    taskENTER_CRITICAL(&s_lock);
    set_text(s_view.name, sizeof(s_view.name), name);
    set_text(s_view.suffix, sizeof(s_view.suffix), suffix);
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_pin(const char *pin)
{
    taskENTER_CRITICAL(&s_lock);
    set_text(s_view.pin, sizeof(s_view.pin), pin);
    s_view.have_pin = (s_view.pin[0] != '\0');
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_clear_pin(void)
{
    taskENTER_CRITICAL(&s_lock);
    memset(s_view.pin, 0, sizeof(s_view.pin));
    s_view.have_pin = false;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_ble_qr(const char *payload)
{
    taskENTER_CRITICAL(&s_lock);
    set_text(s_view.ble_qr, sizeof(s_view.ble_qr), payload);
    s_view.have_ble_qr = (s_view.ble_qr[0] != '\0');
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_wifi_qr(const char *payload)
{
    taskENTER_CRITICAL(&s_lock);
    set_text(s_view.wifi_qr, sizeof(s_view.wifi_qr), payload);
    s_view.have_wifi_qr = (s_view.wifi_qr[0] != '\0');
    taskEXIT_CRITICAL(&s_lock);
}

/**
 * Forget every setup secret at once.
 *
 * Called when the provisioning window closes. The PIN and both QR payloads
 * carry the setup credentials, and a window that has closed is a credential
 * that should no longer be on a screen in a living room -- so they leave RAM at
 * the same moment they leave the display, rather than lingering until the next
 * boot.
 */
void hk_view_clear_setup(void)
{
    taskENTER_CRITICAL(&s_lock);
    memset(s_view.pin, 0, sizeof(s_view.pin));
    memset(s_view.ble_qr, 0, sizeof(s_view.ble_qr));
    memset(s_view.wifi_qr, 0, sizeof(s_view.wifi_qr));
    s_view.have_pin = false;
    s_view.have_ble_qr = false;
    s_view.have_wifi_qr = false;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_rssi(int dbm)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.rssi_dbm = dbm;
    s_view.have_rssi = true;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_clear_rssi(void)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.have_rssi = false;
    s_view.rssi_dbm = 0;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_audio_locked(bool locked)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.audio_locked = locked;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_confirm_setup(bool pending)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.confirm_setup = pending;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_metadata(const hk_view_media_t *media)
{
    if (media == NULL) {
        hk_view_clear_metadata();
        return;
    }
    /* Sanitised on the way in, not on the way out. These strings arrive over
     * the network from whatever is streaming, and the render path is the wrong
     * place to be defending itself: it should be able to assume the strings it
     * draws are terminated and inside their buffers. */
    hk_view_media_t clean = {0};
    set_text(clean.title, sizeof(clean.title), media->title);
    set_text(clean.artist, sizeof(clean.artist), media->artist);
    set_text(clean.album, sizeof(clean.album), media->album);
    clean.duration_secs = media->duration_secs;
    clean.position_secs = media->position_secs;
    clean.position_at_ms = media->position_at_ms;
    clean.has_artwork = media->has_artwork;

    taskENTER_CRITICAL(&s_lock);
    s_view.media = clean;
    s_view.have_metadata = (clean.title[0] != '\0' || clean.artist[0] != '\0');
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_clear_metadata(void)
{
    taskENTER_CRITICAL(&s_lock);
    memset(&s_view.media, 0, sizeof(s_view.media));
    s_view.have_metadata = false;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_volume(int percent, bool muted)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    taskENTER_CRITICAL(&s_lock);
    /* The overlay is triggered by a CHANGE, so an unchanged value must not
     * restamp the clock. A source that republishes the same volume every second
     * would otherwise keep the overlay permanently on screen. */
    const bool changed = (s_view.volume_percent != percent) || (s_view.muted != muted);
    s_view.volume_percent = percent;
    s_view.muted = muted;
    if (changed) {
        s_view.volume_changed_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_battery(int percent, bool charging)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.battery_percent = percent;
    s_view.charging = charging;
    s_view.have_battery = true;
    taskEXIT_CRITICAL(&s_lock);
}

void hk_view_set_power(int current_ma, int pack_mv, bool have_temperature,
                       int temperature_c)
{
    taskENTER_CRITICAL(&s_lock);
    s_view.current_ma = current_ma;
    s_view.have_current = true;
    s_view.pack_mv = pack_mv;
    s_view.have_pack_mv = true;
    s_view.have_temperature = have_temperature;
    s_view.temperature_c = temperature_c;
    taskEXIT_CRITICAL(&s_lock);
}
