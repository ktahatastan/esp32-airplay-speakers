/**
 * @file hk_view.h
 * @brief Everything the screen is allowed to know, in one struct.
 *
 * The LED resolves its colour from hk_led_inputs_t and the screen renders from
 * the same resolution, so the two cannot disagree about what the device is
 * doing (ADR-0017). But a screen can say more than a colour can, and this is
 * where that extra goes: what is playing, how loud, how strong the signal is,
 * what the setup PIN is.
 *
 * Two rules hold this together.
 *
 * First, the screen still has no state. This struct is a snapshot taken under
 * a lock and rendered as a pure function; there is no sequence of commands the
 * screen can miss, which is why a network reset cannot leave it showing the
 * previous thing.
 *
 * Second, every field has exactly one writer, and a field nobody writes is
 * absent rather than zero -- `have_metadata`, `have_pin`, `have_battery`. A
 * screen that shows 0% because nothing has measured the pack yet is lying, and
 * on this project a number that was never measured must never look like one
 * that was.
 */
#ifndef HK_VIEW_H
#define HK_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "hk_led.h"

#define HK_VIEW_TEXT_MAX 64
#define HK_VIEW_PIN_MAX  63
#define HK_VIEW_QR_MAX   192

/** What the AirPlay receiver last reported about the stream. */
typedef struct {
    char     title[HK_VIEW_TEXT_MAX];
    char     artist[HK_VIEW_TEXT_MAX];
    char     album[HK_VIEW_TEXT_MAX];
    uint32_t duration_secs;
    uint32_t position_secs;
    uint32_t position_at_ms;   /**< when position_secs was taken, for coasting */
    bool     has_artwork;
} hk_view_media_t;

typedef struct {
    /* The same inputs the LED resolves from. */
    hk_led_inputs_t led;

    /* Identity. Always present; without it the panel stays dark. */
    char name[HK_VIEW_TEXT_MAX];   /**< "Harman Kardom 932C" */
    char suffix[5];                /**< "932C" */

    /* Setup. Present only while a provisioning window is open. */
    bool have_pin;
    char pin[HK_VIEW_PIN_MAX + 1];
    bool have_ble_qr;
    char ble_qr[HK_VIEW_QR_MAX];   /**< the provisioning payload, verbatim */
    bool have_wifi_qr;
    char wifi_qr[HK_VIEW_QR_MAX];  /**< WIFI:T:WPA;S:...;P:...;; */

    /* Network. */
    bool have_rssi;
    int  rssi_dbm;

    /* Audio. */
    /* Whether the amplifier is allowed to make sound at all. False until a
     * calibration profile exists (G0), and the screen must say so rather than
     * looking idle: a speaker that is silent because it has never been measured
     * and one that is silent because nothing is playing are different things,
     * and only one of them is waiting on the owner. */
    bool            audio_locked;
    bool            have_metadata;
    hk_view_media_t media;
    int             volume_percent;   /**< 0-100 */
    bool            muted;
    uint32_t        volume_changed_ms;/**< 0 if never; drives the overlay */

    /* Power. Absent until something measures it -- see the file comment. */
    bool have_battery;
    int  battery_percent;
    bool charging;
    bool have_current;
    int  current_ma;
    bool have_pack_mv;
    int  pack_mv;
    bool have_temperature;
    int  temperature_c;
} hk_view_t;

/**
 * Take a consistent snapshot. Safe from any task.
 *
 * Consistent matters: rendering directly from the live struct can catch a title
 * half-overwritten by the AirPlay task, and a torn 64-byte string is a screen
 * full of rubbish rather than a slightly stale one.
 */
void hk_view_snapshot(hk_view_t *out);

/* One writer per field. */
void hk_view_set_identity(const char *name, const char *suffix);
void hk_view_set_pin(const char *pin);
void hk_view_clear_pin(void);
void hk_view_set_ble_qr(const char *payload);
void hk_view_set_wifi_qr(const char *payload);
void hk_view_clear_setup(void);
void hk_view_set_rssi(int dbm);
void hk_view_clear_rssi(void);
void hk_view_set_audio_locked(bool locked);
void hk_view_set_metadata(const hk_view_media_t *media);
void hk_view_clear_metadata(void);
void hk_view_set_volume(int percent, bool muted);
void hk_view_set_battery(int percent, bool charging);
void hk_view_set_power(int current_ma, int pack_mv, bool have_temperature,
                       int temperature_c);

#endif /* HK_VIEW_H */
