/**
 * @file hk_icons.h
 * @brief The icon set, as antialiased coverage masks.
 *
 * Generated from the same SVG paths the design uses (tools/gen_icons.py), at
 * the one size each is drawn. Rasterising at build time rather than run time
 * costs flash and buys a frame budget that does not include a path filler.
 */
#ifndef HK_ICONS_H
#define HK_ICONS_H

#include <stdint.h>

typedef struct {
    const char    *name;
    uint8_t        width;
    uint8_t        height;
    const uint8_t *coverage;   /**< 4bpp, two pixels per byte, row-padded */
} hk_icon_t;

typedef enum {
    HK_ICON_AIRPLAY = 0,
    HK_ICON_WIFI,           /**< full strength; hk_icon_wifi_bars() draws a meter */
    HK_ICON_BOLT,
    HK_ICON_VOLT,
    HK_ICON_THERMOMETER,
    HK_ICON_BATTERY,
    HK_ICON_SPEAKER,
    HK_ICON_SPEAKER_MUTE,
    HK_ICON_LOCK,
    HK_ICON_BLUETOOTH,
    HK_ICON_REFRESH,
    HK_ICON_WARNING,
    HK_ICON_COUNT
} hk_icon_id_t;

/** Never NULL: an unknown id returns a zero-size icon that draws nothing. */
const hk_icon_t *hk_icon(hk_icon_id_t id);

/** Draw an icon with its top-left at (x, y). */
void hk_icon_draw(uint16_t *buf, hk_icon_id_t id, int x, int y,
                  uint16_t colour, uint8_t alpha);

/**
 * The signal meter: four bars, `bars` of them lit, the rest at a quarter alpha.
 *
 * Drawn rather than stored, because five states of the same shape as five
 * bitmaps is four bitmaps too many.
 */
void hk_icon_wifi_bars(uint16_t *buf, int x, int y, int bars,
                       uint16_t colour, uint8_t alpha);

/** RSSI in dBm to 0-4 bars. Below -90 is zero; above -55 is four. */
int hk_icon_bars_from_rssi(int rssi_dbm);

#endif /* HK_ICONS_H */
