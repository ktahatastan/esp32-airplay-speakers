/**
 * @file hk_settings.h
 * @brief What the user may change, what it may be set to, and what to do when
 *        the stored value is not one of those things.
 *
 * The interesting case is the third one, and it is easy to get wrong in both
 * directions.
 *
 * A stored value outside its range is not a preference. It is corruption, or a
 * field an older firmware used differently, or a write that was interrupted.
 * CLAMPING it silently applies a number the owner never chose — a volume of
 * 100 becomes the maximum rather than being recognised as nonsense. REFUSING
 * TO BOOT over it is worse: a speaker that will not start because a byte went
 * bad is a speaker nobody can fix without a cable. So the value falls back to
 * its default AND the caller is told it happened, which is the only way the
 * log can explain why the volume changed overnight.
 *
 * The defaults here are product choices, not measurements. That distinction
 * matters in this project: a default volume is something anyone may argue
 * about and change in a line, while a driver protection threshold is a number
 * that has to come off a bench and is refused until it does. Nothing in this
 * file is the second kind.
 */
#ifndef HK_SETTINGS_H
#define HK_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Longest NVS key this project will use.
 *
 * ESP-IDF caps NVS keys at 15 characters plus a terminator, and a longer one
 * is not truncated — the write simply fails. Checked in hk_settings_table_ok()
 * rather than discovered on a device.
 */
#define HK_SETTINGS_KEY_MAX 15u

/** One user-changeable setting. */
typedef struct {
    const char *key;      /**< NVS key */
    const char *summary;  /**< What it does, for diagnostics */
    uint32_t    fallback; /**< Used when absent, unreadable or out of range */
    uint32_t    min;
    uint32_t    max;
} hk_setting_def_t;

/** Why a value came back as it did. */
typedef enum {
    HK_SETTING_STORED = 0,   /**< The stored value was usable */
    HK_SETTING_DEFAULTED,    /**< Nothing was stored */
    HK_SETTING_OUT_OF_RANGE, /**< Something was stored and it was not allowed */
} hk_setting_origin_t;

/** The table, terminated by a NULL key. */
extern const hk_setting_def_t hk_settings_table[];

/** Number of settings, excluding the terminator. */
size_t hk_settings_count(void);

/** Look one up by key, or NULL. */
const hk_setting_def_t *hk_settings_find(const char *key);

/**
 * Turn what was read out of storage into a value that may be used.
 *
 * @param def    the setting; NULL yields 0 and HK_SETTING_DEFAULTED
 * @param stored the value read
 * @param present false when nothing was stored
 * @param origin optional; says whether the answer came from storage, from the
 *               default, or from rejecting what was stored
 */
uint32_t hk_settings_resolve(const hk_setting_def_t *def, uint32_t stored,
                             bool present, hk_setting_origin_t *origin);

/**
 * Whether the table itself is well formed.
 *
 * Keys within the NVS limit, no duplicates, every range ordered, and every
 * default inside its own range — a default outside its range would be rejected
 * by the very function that falls back to it, leaving no usable value at all.
 * A test calls this so a mistake in the table fails on a laptop rather than on
 * a device that then refuses to store anything.
 */
bool hk_settings_table_ok(void);

/**
 * The same checks against any table.
 *
 * The project's own table is correct, which means every check in
 * hk_settings_table_ok() passes for a reason unrelated to whether the check
 * works. This variant lets a test hand over a deliberately broken table and
 * find out.
 */
bool hk_settings_table_check(const hk_setting_def_t *table);

#endif /* HK_SETTINGS_H */
