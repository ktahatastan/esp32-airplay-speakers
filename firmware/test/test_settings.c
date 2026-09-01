#include "hk_test.h"
#include "hk_settings.h"
#include "hk_schema.h"

#include <string.h>

void test_settings(void)
{
    /* ===== the table itself ===== */
    /* Checked here so a mistake fails on a laptop rather than on a device that
     * then quietly refuses to store anything. */
    HK_CHECK(hk_settings_table_ok());
    HK_CHECK(hk_settings_count() > 0);

    for (size_t i = 0; i < hk_settings_count(); i++) {
        const hk_setting_def_t *d = &hk_settings_table[i];
        HK_CHECK(d->key != NULL);
        /* ESP-IDF does not truncate an over-long NVS key, it fails the write. */
        HK_CHECK(strlen(d->key) <= HK_SETTINGS_KEY_MAX);
        HK_CHECK(d->min <= d->fallback && d->fallback <= d->max);
    }

    /* ===== lookup ===== */
    HK_CHECK(hk_settings_find("volume") != NULL);
    HK_CHECK(hk_settings_find("not_a_setting") == NULL);
    HK_CHECK(hk_settings_find(NULL) == NULL);
    HK_CHECK(hk_settings_find("") == NULL);

    {
        const hk_setting_def_t *v = hk_settings_find("volume");
        HK_CHECK(v->min == 0u && v->max == 100u);

        hk_setting_origin_t why = HK_SETTING_STORED;

        /* ===== nothing stored: the default, and said so ===== */
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 0u, false, &why), v->fallback);
        HK_CHECK_EQ_INT(why, HK_SETTING_DEFAULTED);

        /* ===== a stored value inside the range is the user's ===== */
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 55u, true, &why), 55u);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);

        /* the ends of the range are inside it */
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 0u, true, &why), 0u);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 100u, true, &why), 100u);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);

        /* ===== out of range falls back; it does NOT clamp =====
         * Clamping would turn a corrupted byte into "the owner asked for
         * maximum volume", which is both wrong and, at 3 a.m., loud. */
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 101u, true, &why), v->fallback);
        HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);
        HK_CHECK(hk_settings_resolve(v, 101u, true, NULL) != 100u);

        HK_CHECK_EQ_INT(hk_settings_resolve(v, 0xFFFFFFFFu, true, &why), v->fallback);
        HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);

        /* and the reason is distinguishable from "nothing was stored", which
         * is the whole point of reporting it: one is a fresh device, the other
         * is a device whose settings went bad. */
        HK_CHECK(HK_SETTING_OUT_OF_RANGE != HK_SETTING_DEFAULTED);

        /* the origin pointer is optional */
        HK_CHECK_EQ_INT(hk_settings_resolve(v, 55u, true, NULL), 55u);
    }

    /* ===== a boolean setting only takes two values ===== */
    {
        const hk_setting_def_t *a = hk_settings_find("auto_update");
        HK_CHECK(a != NULL);
        HK_CHECK(a->max == 1u);
        hk_setting_origin_t why;
        HK_CHECK_EQ_INT(hk_settings_resolve(a, 1u, true, &why), 1u);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);
        HK_CHECK_EQ_INT(hk_settings_resolve(a, 2u, true, &why), a->fallback);
        HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);
    }

    /* ===== every setting survives its own round trip ===== */
    for (size_t i = 0; i < hk_settings_count(); i++) {
        const hk_setting_def_t *d = &hk_settings_table[i];
        hk_setting_origin_t why;

        HK_CHECK_EQ_INT(hk_settings_resolve(d, d->min, true, &why), d->min);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);
        HK_CHECK_EQ_INT(hk_settings_resolve(d, d->max, true, &why), d->max);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);
        HK_CHECK_EQ_INT(hk_settings_resolve(d, d->fallback, true, &why), d->fallback);

        if (d->max < 0xFFFFFFFFu) {
            HK_CHECK_EQ_INT(hk_settings_resolve(d, d->max + 1u, true, &why), d->fallback);
            HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);
        }
    }

    /* ===== the lower bound, on a setting that has one =====
     * Every setting in the project's own table starts at zero, so the
     * "below minimum" branch is unreachable through it and a test using only
     * that table proves nothing about it. */
    {
        const hk_setting_def_t bounded = {"probe", "a range that excludes zero",
                                          50u, 10u, 90u};
        hk_setting_origin_t why;
        HK_CHECK_EQ_INT(hk_settings_resolve(&bounded, 9u, true, &why), 50u);
        HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);
        HK_CHECK_EQ_INT(hk_settings_resolve(&bounded, 10u, true, &why), 10u);
        HK_CHECK_EQ_INT(why, HK_SETTING_STORED);
        HK_CHECK_EQ_INT(hk_settings_resolve(&bounded, 0u, true, &why), 50u);
        HK_CHECK_EQ_INT(why, HK_SETTING_OUT_OF_RANGE);
        /* and still not clamped to the bound */
        HK_CHECK(hk_settings_resolve(&bounded, 0u, true, NULL) != 10u);
    }

    /* ===== the table checker, against tables that are wrong =====
     * The project's own table passes every check, which says nothing about
     * whether the checks work. */
    {
        const hk_setting_def_t good[] = {
            {"a", "fine", 5u, 0u, 10u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(hk_settings_table_check(good));

        /* a key longer than NVS accepts: the write fails rather than truncating */
        const hk_setting_def_t long_key[] = {
            {"sixteen_char_key", "too long", 0u, 0u, 1u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(long_key));

        /* a default outside its own range would be rejected by the very
         * function that falls back to it, leaving no usable value at all */
        const hk_setting_def_t bad_default[] = {
            {"a", "default above max", 20u, 0u, 10u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(bad_default));

        const hk_setting_def_t bad_default_low[] = {
            {"a", "default below min", 1u, 5u, 10u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(bad_default_low));

        /* an inverted range accepts nothing */
        const hk_setting_def_t inverted[] = {
            {"a", "min above max", 5u, 10u, 0u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(inverted));

        /* two settings on one key: one silently shadows the other */
        const hk_setting_def_t duplicate[] = {
            {"a", "first", 1u, 0u, 10u}, {"a", "second", 2u, 0u, 10u},
            {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(duplicate));

        const hk_setting_def_t empty_key[] = {
            {"", "no key", 0u, 0u, 1u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(empty_key));

        const hk_setting_def_t no_summary[] = {
            {"a", NULL, 0u, 0u, 1u}, {NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(no_summary));

        const hk_setting_def_t empty[] = {{NULL, NULL, 0u, 0u, 0u}};
        HK_CHECK(!hk_settings_table_check(empty));
        HK_CHECK(!hk_settings_table_check(NULL));
    }

    /* ===== a NULL definition yields nothing, not a guess ===== */
    {
        hk_setting_origin_t why = HK_SETTING_STORED;
        HK_CHECK_EQ_INT(hk_settings_resolve(NULL, 42u, true, &why), 0u);
        HK_CHECK_EQ_INT(why, HK_SETTING_DEFAULTED);
    }

    /* ================= schema migration ================= */

    /* ===== deciding to convert is not the same as having a converter =====
     * hk_schema_resolve() answers MIGRATE for anything older, and this build
     * has no converters at all. Acting on that answer would read an old layout
     * as though it were the current one — silently, on all four speakers, the
     * first time anyone bumps a schema version. */
    HK_CHECK(!hk_schema_can_migrate(HK_STORE_USER, 0u));
    HK_CHECK(!hk_schema_can_migrate(HK_STORE_USER, 1u));
    HK_CHECK(!hk_schema_can_migrate(HK_STORE_FACTORY, 0u));
    HK_CHECK(!hk_schema_can_migrate(HK_STORE_FACTORY, 99u));

    /* ===== the migration path, actually reached =====
     * With one schema version there is no valid older one — version 0
     * classifies as corrupt, not as older — so the guard cannot be exercised
     * through hk_schema_plan(). Supplying the current version reaches it, and
     * this is the case that matters: the first time anyone bumps a version,
     * every device in the field takes exactly this path. */
    {
        /* pretend this build writes version 3 and the store holds version 1 */
        HK_CHECK_EQ_INT(hk_schema_classify(true, 1u, 3u), HK_SCHEMA_FOUND_OLDER);
        HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_OLDER),
                        HK_SCHEMA_MIGRATE);

        /* resolve wants to convert; plan refuses because nothing can */
        HK_CHECK_EQ_INT(hk_schema_plan_with(HK_STORE_USER, true, 1u, 3u),
                        HK_SCHEMA_WRITE_DEFAULTS);
        HK_CHECK_EQ_INT(hk_schema_plan_with(HK_STORE_FACTORY, true, 1u, 3u),
                        HK_SCHEMA_FAIL_SAFE);
        HK_CHECK(!hk_schema_audio_permitted(
            hk_schema_plan_with(HK_STORE_FACTORY, true, 1u, 3u)));

        /* and a matching version is untouched by any of this */
        HK_CHECK_EQ_INT(hk_schema_plan_with(HK_STORE_USER, true, 3u, 3u),
                        HK_SCHEMA_USE);
    }

    /* resolve still says MIGRATE... */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_OLDER),
                    HK_SCHEMA_MIGRATE);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_OLDER),
                    HK_SCHEMA_MIGRATE);

    /* ...and plan refuses to pass it on, because nothing can act on it. */
    HK_CHECK(hk_schema_plan(HK_STORE_USER, true, 0u) != HK_SCHEMA_MIGRATE);
    HK_CHECK(hk_schema_plan(HK_STORE_FACTORY, true, 0u) != HK_SCHEMA_MIGRATE);

    /* ===== and the two stores degrade differently ===== */
    /* Losing a volume setting costs a minute. Misreading a driver protection
     * profile costs a tweeter. */
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_USER, true, 0u), HK_SCHEMA_WRITE_DEFAULTS);
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_FACTORY, true, 0u), HK_SCHEMA_FAIL_SAFE);
    HK_CHECK(!hk_schema_audio_permitted(hk_schema_plan(HK_STORE_FACTORY, true, 0u)));

    HK_CHECK_EQ_INT(hk_schema_without_migration(HK_STORE_USER), HK_SCHEMA_WRITE_DEFAULTS);
    HK_CHECK_EQ_INT(hk_schema_without_migration(HK_STORE_FACTORY), HK_SCHEMA_FAIL_SAFE);

    /* ===== plan agrees with resolve everywhere migration is not involved ===== */
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_USER, true, HK_SCHEMA_USER_VERSION),
                    HK_SCHEMA_USE);
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_FACTORY, true, HK_SCHEMA_FACTORY_VERSION),
                    HK_SCHEMA_USE);
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_USER, false, 0u), HK_SCHEMA_WRITE_DEFAULTS);
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_FACTORY, false, 0u), HK_SCHEMA_FAIL_SAFE);
    /* newer than this build: user resets, calibration is read but never written */
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_USER, true, 99u), HK_SCHEMA_WRITE_DEFAULTS);
    HK_CHECK_EQ_INT(hk_schema_plan(HK_STORE_FACTORY, true, 99u), HK_SCHEMA_READ_ONLY);
}
