/**
 * @file hk_screen.c
 * @brief The screens themselves.
 *
 * Layout on a round panel is not layout on a rectangle: the width available to
 * a line depends on how far it sits from the centre. Every vertical position
 * here was chosen against that constraint -- half_width_at() is the function
 * that decides it -- rather than by eye against a square mock. A caption that
 * fits at the middle and is clipped by the bezel at the bottom is the specific
 * mistake this file exists to avoid.
 *
 * Two typefaces, chosen by what the text IS. Words are Inter, because they are
 * language and want a humanist sans. Measured numbers -- volume, pack
 * percentage -- are hk_draw's seven-segment strokes, because they are readings
 * off an instrument and reading them as such is the point. A device id is an
 * identity rather than a measurement, so it is set in Inter with the words.
 */
#include "hk_screen.h"

#include <stdio.h>
#include <string.h>

#include "hk_draw.h"
#include "hk_font.h"
#include "hk_gfx.h"
#include "hk_icons.h"
#include "hk_palette.h"
#include "hk_qr.h"
#include "hk_sky.h"

#define CX 120
#define CY 120

/** Content radius. The panel is 120; the difference is bezel tolerance. */
#define SAFE_R 106

/**
 * Half the width available at `dy` pixels above or below the centre.
 *
 * The whole reason a round layout needs its own arithmetic. Returns 0 outside
 * the circle rather than a negative, so a caller can lay out a line at any y
 * and get "no room" instead of a nonsense width.
 */
static int half_width_at(int dy, int radius)
{
    if (dy < 0) {
        dy = -dy;
    }
    if (dy >= radius) {
        return 0;
    }
    int lo = 0, hi = radius;
    while (lo < hi) {                       /* integer sqrt of r^2 - dy^2 */
        const int mid = (lo + hi + 1) / 2;
        if (mid * mid + dy * dy <= radius * radius) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

/** Ease a 0..1000 progress with a cubic ease-out. Returns 0..1000. */
static int ease_out(int p)
{
    if (p <= 0) {
        return 0;
    }
    if (p >= 1000) {
        return 1000;
    }
    const int inv = 1000 - p;
    return 1000 - (inv * inv * inv) / 1000000;
}

/** Linear 0..1000 progress of `elapsed` through `span`, clamped. */
static int progress(uint32_t elapsed, uint32_t span)
{
    if (span == 0) {
        return 1000;
    }
    if (elapsed >= span) {
        return 1000;
    }
    return (int)((elapsed * 1000u) / span);
}

/** Scale an alpha by a 0..1000 factor. */
static uint8_t fade(uint8_t alpha, int per_mille)
{
    if (per_mille <= 0) {
        return 0;
    }
    if (per_mille >= 1000) {
        return alpha;
    }
    return (uint8_t)((alpha * per_mille) / 1000);
}

/** Centre a measured string on the panel's vertical axis. */
static int centre_x(const hk_face_t *face, const char *text)
{
    return CX - hk_font_measure(face, text) / 2;
}

static void text_centred(uint16_t *buf, const hk_face_t *face, int baseline,
                         const char *text, uint16_t colour, uint8_t alpha)
{
    hk_font_draw(buf, face, centre_x(face, text), baseline, text, colour, alpha);
}

/**
 * The largest of two faces that leaves the circle some margin at this baseline.
 *
 * "Harman Kardom 932C" is 205 px in the display face and the circle offers 210
 * at the name's baseline, so it fits and looks wrong: flush to the bezel with
 * nothing around it. Fitting is not the test -- a round panel needs the margin
 * to read as deliberate, so this asks for eight pixels of it and steps down a
 * face rather than letting a line touch the edge.
 */
static const hk_face_t *fitted_face(const hk_face_t *want, const hk_face_t *fallback,
                                    int baseline, const char *text)
{
    const int room = half_width_at(baseline - CY, SAFE_R) * 2 - 16;
    return (hk_font_measure(want, text) <= room) ? want : fallback;
}

/**
 * A chip: a translucent lozenge with a label, optionally an icon.
 *
 * Returns its width so a row of them can be centred by measuring first and
 * drawing second. Passing x = INT_MIN would have been the cheaper API and the
 * worse one: a caller that forgets to measure gets a row that is off-centre by
 * however wide the last chip happened to be.
 */
static int chip_width(const hk_face_t *face, const char *label, bool icon)
{
    int w = 14;                                   /* padding either side */
    if (icon) {
        w += 13;
    }
    if (label != NULL && label[0] != '\0') {
        w += hk_font_measure(face, label);
    }
    return w;
}

static void chip_draw(uint16_t *buf, int x, int y, int h, const hk_face_t *face,
                      hk_icon_id_t icon, bool has_icon, const char *label,
                      uint16_t tint, uint8_t alpha)
{
    const int w = chip_width(face, label, has_icon);
    hk_gfx_rrect(buf, x, y, w, h, h / 2, HK_C_GLASS, fade(HK_A_GLASS, alpha * 1000 / 255));
    hk_gfx_rrect_outline(buf, x, y, w, h, h / 2, 1, HK_C_RULE,
                         fade(HK_A_RULE, alpha * 1000 / 255));
    int pen = x + 7;
    if (has_icon) {
        const hk_icon_t *ic = hk_icon(icon);
        hk_icon_draw(buf, icon, pen, y + (h - ic->height) / 2, tint, alpha);
        pen += 13;
    }
    if (label != NULL && label[0] != '\0') {
        hk_font_draw(buf, face, pen, y + h / 2 + face->ascent / 2 - 1, label, tint, alpha);
    }
}

/* ------------------------------------------------------------------ */
/*  which screen                                                       */
/* ------------------------------------------------------------------ */

/**
 * Total over hk_led_state_t, deliberately.
 *
 * The screen has no state of its own (ADR-0017), so the only way it can show
 * something nobody designed is for this function to have a gap. It has none:
 * every branch below ends in a screen, and the final return is a real screen
 * rather than a fallback.
 */
hk_screen_id_t hk_screen_choose(const hk_view_t *view)
{
    switch (view->led.button_hold) {
    case HK_BUTTON_HOLD_FACTORY_ARMED:
        return HK_SCREEN_HOLD_FACTORY;
    case HK_BUTTON_HOLD_NETWORK_ARMED:
        return HK_SCREEN_HOLD_NETWORK;
    default:
        break;
    }

    if (view->led.error) {
        return HK_SCREEN_ERROR;
    }
    if (view->led.ota) {
        return HK_SCREEN_OTA;
    }
    if (view->led.battery_low) {
        return HK_SCREEN_BATTERY_LOW;
    }
    if (view->led.playing) {
        return HK_SCREEN_PLAYING;
    }
    if (view->charging && view->have_battery) {
        return HK_SCREEN_CHARGING;
    }
    if (view->led.ready) {
        /* Ready but forbidden to make sound is its own thing, and saying so is
         * the difference between a speaker that is idle and one that is waiting
         * on a measurement its owner has not taken yet. */
        return view->audio_locked ? HK_SCREEN_AUDIO_LOCKED : HK_SCREEN_READY;
    }
    if (view->led.connecting) {
        return HK_SCREEN_CONNECTING;
    }
    if (view->led.provisioning) {
        return (view->have_ble_qr) ? HK_SCREEN_PAIR_BLE : HK_SCREEN_PAIR_AP;
    }
    return HK_SCREEN_BOOT;
}

const char *hk_screen_name(hk_screen_id_t id)
{
    switch (id) {
    case HK_SCREEN_BOOT:         return "boot";
    case HK_SCREEN_PAIR_BLE:     return "pair_ble";
    case HK_SCREEN_PAIR_AP:      return "pair_ap";
    case HK_SCREEN_CONNECTING:   return "connecting";
    case HK_SCREEN_READY:        return "ready";
    case HK_SCREEN_PLAYING:      return "playing";
    case HK_SCREEN_CHARGING:     return "charging";
    case HK_SCREEN_BATTERY_LOW:  return "battery_low";
    case HK_SCREEN_OTA:          return "ota";
    case HK_SCREEN_HOLD_NETWORK: return "hold_network";
    case HK_SCREEN_HOLD_FACTORY: return "hold_factory";
    case HK_SCREEN_AUDIO_LOCKED: return "audio_locked";
    case HK_SCREEN_ERROR:        return "error";
    case HK_SCREEN_COUNT:        break;
    }
    return "unknown";
}

/** The sky each screen sits on. */
static hk_sky_mood_t mood_for(hk_screen_id_t id)
{
    switch (id) {
    case HK_SCREEN_BOOT:         return HK_SKY_BOOT;
    case HK_SCREEN_PAIR_BLE:
    case HK_SCREEN_PAIR_AP:      return HK_SKY_GALAXY;
    case HK_SCREEN_CHARGING:     return HK_SKY_EMBER;
    case HK_SCREEN_HOLD_NETWORK:
    case HK_SCREEN_HOLD_FACTORY:
    case HK_SCREEN_BATTERY_LOW:
    case HK_SCREEN_ERROR:        return HK_SKY_ALERT;
    default:                     return HK_SKY_CALM;
    }
}

/* ------------------------------------------------------------------ */
/*  pieces                                                             */
/* ------------------------------------------------------------------ */

/**
 * The QR code, inside a frame that lets the galaxy through.
 *
 * The frame is translucent on purpose -- the user asked for it, and it is also
 * what keeps the code from reading as a sticker pasted on the sky. The code
 * itself is not: a scanner needs contrast, so the light modules are opaque
 * while everything around them is not.
 */
static void draw_qr(uint16_t *buf, const char *payload, int cx, int cy, int box,
                    uint8_t alpha)
{
    /* Encoded once per payload, not once per frame.
     *
     * The pairing code is on screen for minutes and changes never, but this
     * function is called 24 times a second; re-running Reed-Solomon and an
     * eight-way mask search for an identical answer was measurable on the host
     * before it was ever measured on the part. The cache is keyed on the
     * payload's own bytes rather than a pointer, because the view is a snapshot
     * and its address is a different one every frame. */
    static hk_qr_t  cached;
    static char     cached_for[HK_VIEW_QR_MAX];
    static bool     cached_ok;

    const hk_qr_t *code = NULL;
    if (payload != NULL && payload[0] != '\0') {
        if (strncmp(cached_for, payload, sizeof(cached_for)) != 0) {
            /* strlcpy is not available on every host this file is tested on,
             * and the length is bounded by the view's own field. */
            const size_t length = strnlen(payload, sizeof(cached_for) - 1);
            memcpy(cached_for, payload, length);
            cached_for[length] = '\0';
            cached_ok = hk_qr_encode(cached_for, HK_QR_ECC_MEDIUM, &cached);
        }
        if (cached_ok) {
            code = &cached;
        }
    }

    if (code == NULL) {
        /* Nothing to scan. Say so rather than leaving a hole: an empty frame
         * looks like a code that has not loaded yet, and the owner would wait. */
        hk_gfx_rrect_outline(buf, cx - box / 2, cy - box / 2, box, box, 14, 1,
                             HK_C_RULE, fade(HK_A_RULE, alpha * 1000 / 255));
        text_centred(buf, hk_font_small(), cy + 4, "kod yok", HK_C_INK_3, alpha);
        return;
    }

    /* One module must be a whole number of pixels or the code develops seams a
     * scanner reads as damage. Round down and centre the remainder. */
    const int quiet = 2;                       /* modules of margin, each side */
    const int span  = code->size + 2 * quiet;
    int scale = box / span;
    if (scale < 1) {
        scale = 1;
    }
    const int drawn = span * scale;
    const int x0 = cx - drawn / 2;
    const int y0 = cy - drawn / 2;

    hk_gfx_rrect(buf, x0 - 6, y0 - 6, drawn + 12, drawn + 12, 14,
                 HK_C_GLASS, fade(HK_A_FRAME, alpha * 1000 / 255));
    hk_gfx_rrect_outline(buf, x0 - 6, y0 - 6, drawn + 12, drawn + 12, 14, 1,
                         HK_C_ACCENT, fade(110, alpha * 1000 / 255));

    /* The light field is drawn opaque and the dark modules on top of it. The
     * other order -- dark modules over the galaxy -- gives a scanner a light
     * field that is whatever star happened to be behind it. */
    hk_gfx_rrect(buf, x0, y0, drawn, drawn, 6, HK_C_STARLIGHT, alpha);
    for (int my = 0; my < code->size; my++) {
        for (int mx = 0; mx < code->size; mx++) {
            if (!hk_qr_module(code, mx, my)) {
                continue;
            }
            hk_gfx_rect(buf, x0 + (mx + quiet) * scale, y0 + (my + quiet) * scale,
                        scale, scale, HK_C_VOID, alpha);
        }
    }
}

/**
 * The setup key, in the form its length can carry.
 *
 * Per-character cells are the design and they work for a short PIN: four digits
 * in four boxes is a thing you read once and type. Twelve characters in twelve
 * boxes is 319 px wide on a 240 px panel, so past six the cells collapse into
 * one frame and the key is set as a word instead.
 *
 * The device ships with a twelve-character key today, so the second branch is
 * the one that runs -- the four-digit PIN the owner asked for changes what the
 * WPA2 network is keyed with and needs its own decision, because WPA2 will not
 * take a passphrase shorter than eight characters.
 */
static void draw_pin(uint16_t *buf, const char *pin, int cy, uint8_t alpha)
{
    const int count = (int)strnlen(pin, HK_VIEW_PIN_MAX);
    if (count <= 0) {
        return;
    }
    if (count > 6) {
        const int w = hk_font_measure(hk_font_body(), pin) + 22;
        const int room = half_width_at(cy - CY, SAFE_R) * 2;
        const int box = (room > 0 && w > room) ? room : w;
        hk_gfx_rrect(buf, CX - box / 2, cy - 16, box, 32, 9, HK_C_GLASS,
                     fade(HK_A_FRAME, alpha * 1000 / 255));
        hk_gfx_rrect_outline(buf, CX - box / 2, cy - 16, box, 32, 9, 1, HK_C_ACCENT,
                             fade(90, alpha * 1000 / 255));
        hk_gfx_clip_set(CX - box / 2 + 4, cy - 16, box - 8, 32);
        text_centred(buf, hk_font_body(), cy + 5, pin, HK_C_INK, alpha);
        hk_gfx_clip_reset();
        return;
    }
    const int cw = 22, ch = 28, gap = 5;
    const int total = count * cw + (count - 1) * gap;
    int x = CX - total / 2;
    for (int i = 0; i < count; i++) {
        hk_gfx_rrect(buf, x, cy - ch / 2, cw, ch, 7, HK_C_GLASS,
                     fade(HK_A_FRAME, alpha * 1000 / 255));
        hk_gfx_rrect_outline(buf, x, cy - ch / 2, cw, ch, 7, 1, HK_C_ACCENT,
                             fade(90, alpha * 1000 / 255));
        const char one[2] = {pin[i], '\0'};
        hk_font_draw(buf, hk_font_display(),
                     x + cw / 2 - hk_font_measure(hk_font_display(), one) / 2,
                     cy + hk_font_display()->ascent / 2,
                     one, HK_C_INK, alpha);
        x += cw + gap;
    }
}

/**
 * Text that scrolls only if it has to.
 *
 * Measured, not guessed: a title that fits is drawn still, because a short
 * title sliding back and forth for no reason is the thing that makes a screen
 * feel cheap. One that does not fit scrolls, pausing at the head so the
 * beginning is readable, and dissolves into both edges rather than being cut.
 */
static void draw_marquee(uint16_t *buf, const hk_face_t *face, int baseline,
                         int box_x, int box_w, const char *text,
                         uint16_t colour, uint8_t alpha, uint32_t t_ms)
{
    /* The box the caller asked for, narrowed to what the circle actually has at
     * this baseline. A marquee sized for the middle of the panel and drawn near
     * the bottom would run under the bezel. */
    const int room = half_width_at(baseline - CY, SAFE_R) * 2;
    if (room <= 0) {
        return;
    }
    if (box_w > room) {
        box_x += (box_w - room) / 2;
        box_w = room;
    }
    const int width = hk_font_measure(face, text);
    if (width <= box_w) {
        hk_font_draw(buf, face, box_x + (box_w - width) / 2, baseline, text, colour, alpha);
        return;
    }

    const int gap = 34;                          /* space between repeats */
    const int cycle = width + gap;
    const uint32_t pause_ms = 1400;
    const uint32_t travel_ms = (uint32_t)cycle * 1000u / 30u;   /* 30 px/s */
    const uint32_t period = pause_ms + travel_ms;
    const uint32_t p = t_ms % period;
    const int shift = (p < pause_ms) ? 0 : (int)(((p - pause_ms) * (uint32_t)cycle) / travel_ms);

    hk_gfx_clip_set(box_x, baseline - face->ascent - 2, box_w, face->line_height + 4);
    hk_font_draw_faded(buf, face, box_x - shift, baseline, text, colour, alpha,
                       box_x, box_w, 12, 12);
    hk_font_draw_faded(buf, face, box_x - shift + cycle, baseline, text, colour, alpha,
                       box_x, box_w, 12, 12);
    hk_gfx_clip_reset();
}

/** Clamp a reading to what this hardware can physically produce. */
static int clamp_reading(int value, int lo, int hi)
{
    return (value < lo) ? lo : ((value > hi) ? hi : value);
}

/** mm:ss, saturating rather than wrapping. */
static void format_time(uint32_t seconds, char *out, size_t size)
{
    if (seconds > 5999u) {
        seconds = 5999u;
    }
    snprintf(out, size, "%u:%02u", (unsigned)(seconds / 60u), (unsigned)(seconds % 60u));
}

/**
 * Where the track is now.
 *
 * The receiver reports a position when it feels like it, not every frame, so a
 * progress ring driven straight from that value jumps. This coasts from the
 * last report using the clock, which is what makes the ring move smoothly, and
 * clamps at the duration so a stale report cannot run it past the end.
 */
static uint32_t coasted_position(const hk_view_media_t *media, uint32_t t_ms)
{
    uint32_t position = media->position_secs;
    if (media->position_at_ms != 0 && t_ms > media->position_at_ms) {
        position += (t_ms - media->position_at_ms) / 1000u;
    }
    if (media->duration_secs > 0 && position > media->duration_secs) {
        position = media->duration_secs;
    }
    return position;
}

/**
 * Cover art, generated.
 *
 * The receiver tells us artwork EXISTS (rtsp_metadata_t.has_artwork) but does
 * not hand us the image, so there is nothing to display and drawing a generic
 * music note would be a placeholder pretending to be a cover. This is instead
 * derived from the track's own text: the same title always produces the same
 * figure, so it works like a colour a listener comes to associate with a piece.
 */
static void draw_cover(uint16_t *buf, const hk_view_media_t *media, int x, int y,
                       int size, uint32_t t_ms, uint8_t alpha)
{
    uint32_t h = 2166136261u;
    for (const char *s = media->title; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    for (const char *s = media->artist; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }

    hk_gfx_rrect(buf, x, y, size, size, 12, HK_C_VOID, alpha);
    const uint16_t tints[4] = {HK_C_NEBULA, HK_C_DUST, HK_C_ACCENT, HK_C_CORE};
    const int cx = x + size / 2, cy = y + size / 2;
    const uint8_t breath = hk_sky_breath(t_ms);

    hk_gfx_clip_set(x, y, size, size);
    for (int i = 0; i < 4; i++) {
        const uint32_t bits = h >> (i * 7);
        const int bx = x + (int)(bits % (uint32_t)size);
        const int by = y + (int)((bits >> 3) % (uint32_t)size);
        const int r  = size / 5 + (int)((bits >> 6) % (uint32_t)(size / 4));
        hk_gfx_disc(buf, HK_Q4(bx), HK_Q4(by), HK_Q4(r), tints[i & 3],
                    fade(150, alpha * 1000 / 255));
    }
    /* One ring that breathes with everything else, so the cover belongs to the
     * same screen rather than sitting on it. */
    hk_gfx_ring(buf, HK_Q4(cx), HK_Q4(cy), HK_Q4(size / 3), HK_Q4(size / 3 - 2),
                HK_C_STARLIGHT, fade(60 + breath / 3, alpha * 1000 / 255));
    hk_gfx_clip_reset();
    hk_gfx_rrect_outline(buf, x, y, size, size, 12, 1, HK_C_RULE,
                         fade(HK_A_RULE, alpha * 1000 / 255));
}

/* ------------------------------------------------------------------ */
/*  the screens                                                        */
/* ------------------------------------------------------------------ */

static void screen_boot(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                        uint32_t entered_ms, uint8_t alpha)
{
    (void)t_ms;
    /* The name arrives after the stars have gathered, not with them. */
    const int in = ease_out(progress(entered_ms > 700 ? entered_ms - 700 : 0, 700));
    const uint8_t a = fade(alpha, in);
    const int rise = 8 - (8 * in) / 1000;

    text_centred(buf, fitted_face(hk_font_display(), hk_font_body(), 124 + rise,
                                  "Harman Kardom"),
                 124 + rise, "Harman Kardom", HK_C_INK, a);
    text_centred(buf, hk_font_small(), 148 + rise,
                 view->suffix[0] != '\0' ? view->suffix : "…", HK_C_INK_3, a);
}

static void screen_pair_ble(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                            uint32_t entered_ms, uint8_t alpha)
{
    (void)t_ms;
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));

    /* The code gets the panel, and everything else gets out of its way.
     *
     * A 41-module code has to land on a whole number of pixels per module or a
     * scanner reads the seams as damage, so the size goes up in steps: at a
     * 108 px box it was drawing at 2 px per module -- 90 px on a 240 px panel,
     * which is what "too small" looked like. The largest square that fits
     * inside the content circle is about 150 px, and that is 3 px per module.
     *
     * Getting there costs the name line below, and that is the right trade:
     * the payload already carries the device name, so the line was telling the
     * owner something their phone was about to tell them anyway. What they
     * cannot get from anywhere else is a code big enough to scan, so the
     * identity moves up into the chip instead. */
    char label[24];
    snprintf(label, sizeof(label), "BLE · %s",
             view->suffix[0] != '\0' ? view->suffix : "kur");
    chip_draw(buf, CX - chip_width(hk_font_small(), label, true) / 2, 14, 18,
              hk_font_small(), HK_ICON_BLUETOOTH, true, label, HK_C_ACCENT, a);
    draw_qr(buf, view->ble_qr, CX, 124, 150, a);
}

static void screen_pair_ap(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                           uint32_t entered_ms, uint8_t alpha)
{
    (void)t_ms;
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));

    chip_draw(buf, CX - chip_width(hk_font_small(), "Wi-Fi ile kur", true) / 2,
              12, 18, hk_font_small(), HK_ICON_WIFI, true, "Wi-Fi ile kur",
              HK_C_ACCENT, a);

    if (view->have_wifi_qr) {
        /* Shorter payload, fewer modules, so this one reaches 4 px per module
         * in the same space the pairing code manages 3. */
        draw_qr(buf, view->wifi_qr, CX, 112, 132, a);
        if (view->have_pin) {
            draw_pin(buf, view->pin, 198, a);
        }
    } else if (view->have_pin) {
        text_centred(buf, hk_font_small(), 108, "kurulum ağına katıl", HK_C_INK_3, a);
        draw_pin(buf, view->pin, 140, a);
        text_centred(buf, hk_font_small(), 182,
                     view->name[0] != '\0' ? view->name : "Harman Kardom", HK_C_INK_2, a);
    } else {
        text_centred(buf, hk_font_body(), 128, "kurulum açık", HK_C_INK, a);
    }
}

static void screen_connecting(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                              uint32_t entered_ms, uint8_t alpha)
{
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));
    /* The meter fills rather than reporting: nothing has associated yet, so
     * there is no signal strength to show and a static four bars would be a
     * measurement we do not have. */
    const int bars = (int)((t_ms / 400u) % 5u);
    hk_icon_wifi_bars(buf, CX - 11, 96, bars, HK_C_WAIT, a);
    text_centred(buf, hk_font_body(), 142, "ağa katılıyor", HK_C_INK, a);
    text_centred(buf, hk_font_small(), 164,
                 view->name[0] != '\0' ? view->name : "Harman Kardom", HK_C_INK_3, a);
}

static void screen_ready(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                         uint32_t entered_ms, uint8_t alpha)
{
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));
    const uint8_t breath = hk_sky_breath(t_ms);

    /* A ring that breathes with the sky, so an idle speaker still looks alive
     * without anything blinking at the owner. */
    hk_gfx_ring(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(112), HK_Q4(110), HK_C_ACCENT,
                fade((uint8_t)(40 + breath / 3), alpha * 1000 / 255));

    if (view->have_rssi) {
        hk_icon_wifi_bars(buf, CX - 11, 84, hk_icon_bars_from_rssi(view->rssi_dbm),
                          HK_C_GOOD, a);
    }
    const char *name = view->name[0] != '\0' ? view->name : "Harman Kardom";
    text_centred(buf, fitted_face(hk_font_display(), hk_font_body(), 134, name), 134,
                 name, HK_C_INK, a);
    text_centred(buf, hk_font_small(), 158, "AirPlay hazır", HK_C_INK_3, a);
}

static void screen_playing(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                           uint32_t entered_ms, uint8_t alpha)
{
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));
    const hk_view_media_t *m = &view->media;

    /* Progress on the outer edge, where it is legible from across a room and
     * costs the middle of the screen nothing. */
    const uint32_t position = coasted_position(m, t_ms);
    hk_gfx_ring(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(114), HK_Q4(110), HK_C_RULE,
                fade(160, alpha * 1000 / 255));
    if (m->duration_secs > 0) {
        hk_gfx_arc(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(114), HK_Q4(110), 0,
                   (int)((position * 1000u) / m->duration_secs), HK_C_CORE, a);
    }

    /* The badge row sits at y = 30: at y = 26 the circle is only 149 px wide and
     * three chips do not fit, which is exactly how this row fell apart once. */
    char vol[8];
    snprintf(vol, sizeof(vol), "%d", view->volume_percent);
    const int row = chip_width(hk_font_small(), NULL, true) + 6 +
                    chip_width(hk_font_small(), vol, true);
    int x = CX - row / 2;
    chip_draw(buf, x, 30, 18, hk_font_small(), HK_ICON_AIRPLAY, true, NULL, HK_C_ACCENT, a);
    x += chip_width(hk_font_small(), NULL, true) + 6;
    chip_draw(buf, x, 30, 18, hk_font_small(),
              view->muted ? HK_ICON_SPEAKER_MUTE : HK_ICON_SPEAKER, true, vol,
              view->muted ? HK_C_INK_3 : HK_C_INK_2, a);

    draw_cover(buf, m, CX - 34, 58, 68, t_ms, a);

    draw_marquee(buf, hk_font_body(), 152, CX - 92, 184,
                 m->title[0] != '\0' ? m->title : "—", HK_C_INK, a, t_ms);
    if (m->artist[0] != '\0') {
        draw_marquee(buf, hk_font_small(), 172, CX - 74, 148, m->artist,
                     HK_C_INK_2, a, t_ms);
    }

    if (m->duration_secs > 0) {
        char now[8], total[8], line[20];
        format_time(position, now, sizeof(now));
        format_time(m->duration_secs, total, sizeof(total));
        snprintf(line, sizeof(line), "%s / %s", now, total);
        text_centred(buf, hk_font_small(), 194, line, HK_C_INK_3, a);
    }
}

static void screen_charging(uint16_t *buf, const hk_view_t *view, uint32_t t_ms,
                            uint32_t entered_ms, uint8_t alpha)
{
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 320)));
    char text[16];

    hk_icon_draw(buf, HK_ICON_BATTERY, CX - 20, 62, HK_C_CORE, a);
    hk_icon_draw(buf, HK_ICON_BOLT, CX + 10, 62, HK_C_CORE,
                 fade(a, 500 + hk_sky_breath(t_ms) * 500 / 255));

    snprintf(text, sizeof(text), "%d", clamp_reading(view->battery_percent, 0, 100));
    const int gw = 22, gh = 38, th = 4;
    const int n = (int)strlen(text);
    const int digits = hk_draw_text_width(gw, th, n);
    const int suffix = hk_font_measure(hk_font_body(), "%");
    /* Centre the pair, not the digits: centring the number and hanging the sign
     * off its right edge puts the whole reading off-axis by half a glyph. */
    const int left = CX - (digits + 5 + suffix) / 2;
    hk_draw_text(buf, left, 100, gw, gh, th, text, HK_C_INK);
    hk_font_draw(buf, hk_font_body(), left + digits + 5, 100 + gh, "%", HK_C_INK_3, a);

    /* Only what something measured. A row of dashes is honest; a plausible
     * number is not, and on this project that distinction is the whole point of
     * the have_* flags. */
    int metrics = 0;
    char amps[12] = {0}, volts[12] = {0}, temp[12] = {0};
    /* Clamped to what this hardware can physically produce before it is
     * formatted, not after. A sensor that reports nonsense is a fault to
     * report, but the panel must not be where the nonsense first shows up --
     * and a wider buffer would only hide the same problem one digit later. */
    if (view->have_current) {
        const int ma = clamp_reading(view->current_ma, -9999, 9999);
        snprintf(amps, sizeof(amps), "%d.%02d A", ma / 1000, (ma < 0 ? -ma : ma) % 1000 / 10);
        metrics += chip_width(hk_font_small(), amps, true) + 5;
    }
    if (view->have_pack_mv) {
        /* A 4S lithium pack lives between roughly 10 V and 17 V; the range here
         * is wider than that on purpose, so a real fault is still legible. */
        const int mv = clamp_reading(view->pack_mv, 0, 99999);
        snprintf(volts, sizeof(volts), "%d.%d V", mv / 1000, (mv % 1000) / 100);
        metrics += chip_width(hk_font_small(), volts, true) + 5;
    }
    if (view->have_temperature) {
        snprintf(temp, sizeof(temp), "%d °C", clamp_reading(view->temperature_c, -99, 199));
        metrics += chip_width(hk_font_small(), temp, true) + 5;
    }
    int x = CX - (metrics > 0 ? metrics - 5 : 0) / 2;
    if (amps[0] != '\0') {
        chip_draw(buf, x, 156, 17, hk_font_small(), HK_ICON_BOLT, true, amps, HK_C_INK_2, a);
        x += chip_width(hk_font_small(), amps, true) + 5;
    }
    if (volts[0] != '\0') {
        chip_draw(buf, x, 156, 17, hk_font_small(), HK_ICON_VOLT, true, volts, HK_C_INK_2, a);
        x += chip_width(hk_font_small(), volts, true) + 5;
    }
    if (temp[0] != '\0') {
        chip_draw(buf, x, 156, 17, hk_font_small(), HK_ICON_THERMOMETER, true, temp,
                  view->temperature_c >= 45 ? HK_C_EMBER : HK_C_INK_2, a);
    }

    /* Not a reminder -- the rule itself. ADR-0004. */
    chip_draw(buf, CX - chip_width(hk_font_small(), "ses kapalı · şarjda", true) / 2,
              184, 17, hk_font_small(), HK_ICON_SPEAKER_MUTE, true,
              "ses kapalı · şarjda", HK_C_INK_3, a);
}

/**
 * One shape for every state that is a warning, in four colours and four words.
 *
 * These states share a structure because they share a job: something is wrong
 * or about to happen, and the owner has to understand it in the second before
 * they act. Four separate layouts would mean four chances for one of them to be
 * the one nobody looked at.
 */
static void screen_notice(uint16_t *buf, uint32_t t_ms, uint32_t entered_ms,
                          uint8_t alpha, hk_icon_id_t icon, uint16_t tint,
                          const char *headline, const char *detail,
                          int ring_per_mille, bool pulse)
{
    const uint8_t a = fade(alpha, ease_out(progress(entered_ms, 260)));
    const uint8_t breath = hk_sky_breath(t_ms);

    hk_gfx_ring(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(112), HK_Q4(107), tint,
                fade(pulse ? (uint8_t)(70 + breath / 2) : 60, alpha * 1000 / 255));
    if (ring_per_mille > 0) {
        hk_gfx_arc(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(112), HK_Q4(107), 0,
                   ring_per_mille, tint, a);
    }

    const hk_icon_t *ic = hk_icon(icon);
    hk_icon_draw(buf, icon, CX - ic->width / 2, 84, tint, a);
    text_centred(buf, hk_font_display(), 140, headline, HK_C_INK, a);
    if (detail != NULL) {
        text_centred(buf, hk_font_small(), 164, detail, HK_C_INK_3, a);
    }
}

/* ------------------------------------------------------------------ */
/*  entry point                                                        */
/* ------------------------------------------------------------------ */

void hk_screen_render(uint16_t *buf, hk_screen_id_t id, const hk_view_t *view,
                      uint32_t t_ms, uint32_t entered_ms, uint8_t level)
{
    hk_gfx_clip_reset();
    hk_sky_render(buf, t_ms, mood_for(id), level);

    switch (id) {
    case HK_SCREEN_BOOT:
        screen_boot(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_PAIR_BLE:
        screen_pair_ble(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_PAIR_AP:
        screen_pair_ap(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_CONNECTING:
        screen_connecting(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_READY:
        screen_ready(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_PLAYING:
        screen_playing(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_CHARGING:
        screen_charging(buf, view, t_ms, entered_ms, level);
        break;
    case HK_SCREEN_BATTERY_LOW:
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_BATTERY, HK_C_ALARM,
                      "batarya düşük", "şarja takın", 0, true);
        break;
    case HK_SCREEN_OTA:
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_REFRESH, HK_C_ACCENT,
                      "güncelleniyor", "gücü kesmeyin",
                      (int)((t_ms / 4u) % 1000u), false);
        break;
    case HK_SCREEN_HOLD_NETWORK:
        /* The ring fills over the seven seconds between this level and the
         * next, so it is showing a real deadline: keep holding and this becomes
         * a factory reset. HK_BUTTON_NETWORK_MS to HK_BUTTON_FACTORY_MS. */
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_WIFI, HK_C_WAIT,
                      "ağ sıfırlanacak", "bırakın · Wi-Fi silinir",
                      progress(entered_ms,
                               HK_BUTTON_FACTORY_MS - HK_BUTTON_NETWORK_MS),
                      false);
        break;
    case HK_SCREEN_HOLD_FACTORY:
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_WARNING, HK_C_ALARM,
                      "fabrika ayarları", "bırakın · ayarlar silinir", 1000, true);
        break;
    case HK_SCREEN_AUDIO_LOCKED:
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_LOCK, HK_C_WAIT,
                      "ses kilitli", "kalibrasyon yok · G0", 0, true);
        break;
    case HK_SCREEN_ERROR:
    case HK_SCREEN_COUNT:
    default:
        screen_notice(buf, t_ms, entered_ms, level, HK_ICON_WARNING, HK_C_ALARM,
                      "hata", "yeniden başlatın", 0, true);
        break;
    }

    /* No vignette pass here, and that is a measurement rather than a
     * preference. hk_sky_render() already applies one from its own radius table
     * while it writes each pixel for the first time; adding a second pass cost
     * a full read-modify-write over 57,600 PSRAM pixels and showed up as a
     * 112 ms frame on the first board it ran on -- for an effect that was
     * already there. Foreground content stays inside SAFE_R, so it never
     * reaches the part of the glass a vignette would have darkened. */
}

/**
 * The volume overlay.
 *
 * It enters fast and leaves slowly. A control that appears sluggishly feels
 * broken -- the user has already turned the knob -- while one that vanishes
 * abruptly reads as a glitch, so the two halves of its life get different
 * curves rather than one symmetric fade.
 */
void hk_screen_volume_overlay(uint16_t *buf, const hk_view_t *view, uint32_t age_ms)
{
    if (age_ms >= HK_SCREEN_VOLUME_MS) {
        return;
    }
    const uint32_t enter = 140, leave = 420;
    int in;
    if (age_ms < enter) {
        in = ease_out(progress(age_ms, enter));
    } else if (age_ms > HK_SCREEN_VOLUME_MS - leave) {
        in = 1000 - progress(age_ms - (HK_SCREEN_VOLUME_MS - leave), leave);
    } else {
        in = 1000;
    }
    const uint8_t a = fade(255, in);

    /* The screen behind it is dimmed rather than covered: the owner should
     * still see what was playing while they change how loud it is. */
    hk_gfx_disc(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(120), HK_C_VOID, fade(190, in));

    const int radius = 92 - (12 * (1000 - in)) / 1000;
    hk_gfx_ring(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(radius), HK_Q4(radius - 6),
                HK_C_RULE, fade(200, in));
    if (!view->muted && view->volume_percent > 0) {
        hk_gfx_arc(buf, HK_Q4(CX), HK_Q4(CY), HK_Q4(radius), HK_Q4(radius - 6), 0,
                   view->volume_percent * 10, HK_C_ACCENT, a);
    }

    hk_icon_draw(buf, view->muted ? HK_ICON_SPEAKER_MUTE : HK_ICON_SPEAKER,
                 CX - hk_icon(HK_ICON_SPEAKER)->width / 2, 78,
                 view->muted ? HK_C_INK_3 : HK_C_ACCENT, a);

    if (view->muted) {
        text_centred(buf, hk_font_display(), 146, "sessiz", HK_C_INK_2, a);
    } else {
        char text[8];
        snprintf(text, sizeof(text), "%d", clamp_reading(view->volume_percent, 0, 100));
        const int gw = 24, gh = 40, th = 5;
        hk_draw_text(buf, CX - hk_draw_text_width(gw, th, (int)strlen(text)) / 2,
                     106, gw, gh, th, text, HK_C_INK);
    }
}
