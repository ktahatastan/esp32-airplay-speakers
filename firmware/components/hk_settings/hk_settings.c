#include "hk_settings.h"

#include <string.h>

/*
 * Product defaults. Deliberately conservative where the consequence of being
 * wrong is loud: a speaker that starts quiet is an annoyance, one that starts
 * at full volume in a bedroom at 3 a.m. is a different kind of problem.
 */
const hk_setting_def_t hk_settings_table[] = {
    {"volume", "playback volume, percent", 30u, 0u, 100u},
    {"led_bright", "status LED brightness, percent", 60u, 0u, 100u},
    {"auto_update", "check for updates automatically", 1u, 0u, 1u},
    {"standby_min", "idle minutes before power save; 0 disables", 30u, 0u, 240u},
    /* Which release channel this speaker follows. Stored rather than compiled
     * in, because the canary step in the OTA plan puts ONE device on the
     * candidate channel — and rebuilding a different image for it would mean
     * the canary is not testing the image the others will get. */
    {"channel", "0 = stable, 1 = canary", 0u, 0u, 1u},
    /* How many times an update has been rolled back. Survives the reboot a
     * rollback causes, which is the only reason it is in storage at all. */
    {"rollbacks", "consecutive rolled-back updates", 0u, 0u, 255u},
    {NULL, NULL, 0u, 0u, 0u},
};

size_t hk_settings_count(void)
{
    size_t n = 0;
    while (hk_settings_table[n].key != NULL) {
        n++;
    }
    return n;
}

const hk_setting_def_t *hk_settings_find(const char *key)
{
    if (key == NULL) {
        return NULL;
    }
    for (size_t i = 0; hk_settings_table[i].key != NULL; i++) {
        if (strcmp(hk_settings_table[i].key, key) == 0) {
            return &hk_settings_table[i];
        }
    }
    return NULL;
}

uint32_t hk_settings_resolve(const hk_setting_def_t *def, uint32_t stored,
                             bool present, hk_setting_origin_t *origin)
{
    hk_setting_origin_t why = HK_SETTING_DEFAULTED;
    uint32_t value = 0u;

    if (def == NULL) {
        if (origin != NULL) {
            *origin = why;
        }
        return value;
    }

    if (!present) {
        value = def->fallback;
    } else if (stored < def->min || stored > def->max) {
        /* Not clamped. Clamping would apply a number the owner never chose and
         * make corruption look like a preference; the default at least is a
         * value somebody decided on. */
        value = def->fallback;
        why = HK_SETTING_OUT_OF_RANGE;
    } else {
        value = stored;
        why = HK_SETTING_STORED;
    }

    if (origin != NULL) {
        *origin = why;
    }
    return value;
}

bool hk_settings_table_check(const hk_setting_def_t *table)
{
    if (table == NULL) {
        return false;
    }
    size_t count = 0;
    for (size_t i = 0; table[i].key != NULL; i++) {
        const hk_setting_def_t *a = &table[i];
        count++;

        const size_t len = strlen(a->key);
        if (len == 0u || len > HK_SETTINGS_KEY_MAX) {
            return false;
        }
        if (a->summary == NULL) {
            return false;
        }
        /* A default outside its own range would be rejected by the function
         * that falls back to it, leaving nothing usable.
         *
         * This also covers an inverted range, which is why there is no
         * separate min > max check: if min exceeds max then every possible
         * default is either below the minimum or above the maximum, so this
         * one comparison rejects it. A second check would look like more
         * safety while being unreachable, and no test could tell whether it
         * worked. */
        if (a->fallback < a->min || a->fallback > a->max) {
            return false;
        }
        for (size_t j = i + 1u; table[j].key != NULL; j++) {
            if (strcmp(a->key, table[j].key) == 0) {
                return false;
            }
        }
    }
    return count > 0u;
}

bool hk_settings_table_ok(void)
{
    return hk_settings_table_check(hk_settings_table);
}
