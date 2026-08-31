#include "hk_test.h"
#include "hk_schema.h"

/*
 * PRD-008 and the storage rules in docs/03-firmware/firmware-plan.md.
 *
 * The whole point of this module is the asymmetry between the two stores, so
 * the tests are organised around it: the same input must produce a forgiving
 * answer for user settings and a cautious one for calibration.
 */
void test_schema(void)
{
    /* --- classification --- */
    HK_CHECK_EQ_INT(hk_schema_classify(false, 0, 1), HK_SCHEMA_FOUND_ABSENT);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 1, 1), HK_SCHEMA_FOUND_MATCH);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 1, 3), HK_SCHEMA_FOUND_OLDER);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 4, 3), HK_SCHEMA_FOUND_NEWER);
    /* Zero is never written as a version, so reading it means the slot held
     * something else. Same for a value too large to be a version. */
    HK_CHECK_EQ_INT(hk_schema_classify(true, 0, 1), HK_SCHEMA_FOUND_CORRUPT);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 0x10000, 1), HK_SCHEMA_FOUND_CORRUPT);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 0xFFFFFFFF, 1), HK_SCHEMA_FOUND_CORRUPT);
    HK_CHECK_EQ_INT(hk_schema_classify(true, 0xFFFF, 1), HK_SCHEMA_FOUND_NEWER);

    /* --- user settings are recoverable, so the priority is booting --- */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_MATCH), HK_SCHEMA_USE);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_OLDER), HK_SCHEMA_MIGRATE);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_ABSENT),
                    HK_SCHEMA_WRITE_DEFAULTS);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_CORRUPT),
                    HK_SCHEMA_WRITE_DEFAULTS);
    /* The rollback case: newer firmware wrote settings this build cannot read.
     * Starting from defaults beats refusing to start. */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_USER, HK_SCHEMA_FOUND_NEWER),
                    HK_SCHEMA_WRITE_DEFAULTS);

    /* --- calibration is not recoverable, so nothing is ever discarded --- */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_MATCH), HK_SCHEMA_USE);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_OLDER), HK_SCHEMA_MIGRATE);

    /* Newer: read what we understand, write nothing. Overwriting would destroy
     * a profile that cost bench time to produce. */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_NEWER),
                    HK_SCHEMA_READ_ONLY);

    /* Absent or corrupt: fail safe, and specifically NOT write_defaults. A
     * default profile nobody measured would look exactly like a working one
     * while driving unprotected tweeters. */
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_ABSENT),
                    HK_SCHEMA_FAIL_SAFE);
    HK_CHECK_EQ_INT(hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_CORRUPT),
                    HK_SCHEMA_FAIL_SAFE);

    /* Stated as an invariant, so a future edit cannot quietly introduce it:
     * calibration is never initialised from defaults and never erased. */
    for (int f = HK_SCHEMA_FOUND_ABSENT; f <= HK_SCHEMA_FOUND_CORRUPT; f++) {
        hk_schema_action_t action = hk_schema_resolve(HK_STORE_FACTORY, (hk_schema_found_t)f);
        HK_CHECK(action != HK_SCHEMA_WRITE_DEFAULTS);
    }

    /* --- what may drive audio --- */
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(HK_SCHEMA_USE), 1);
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(HK_SCHEMA_MIGRATE), 1);
    /* Read-only is still a profile that was actually measured. */
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(HK_SCHEMA_READ_ONLY), 1);
    /* These two mean no trustworthy profile exists. */
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(HK_SCHEMA_FAIL_SAFE), 0);
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(HK_SCHEMA_WRITE_DEFAULTS), 0);

    /* The composed rule that matters: an uncalibrated or unreadable device must
     * never be permitted to drive its drivers at normal levels. */
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(
                        hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_ABSENT)), 0);
    HK_CHECK_EQ_INT(hk_schema_audio_permitted(
                        hk_schema_resolve(HK_STORE_FACTORY, HK_SCHEMA_FOUND_CORRUPT)), 0);

    /* --- the versions this firmware writes are sane --- */
    HK_CHECK(HK_SCHEMA_USER_VERSION > 0);
    HK_CHECK(HK_SCHEMA_FACTORY_VERSION > 0);

    HK_CHECK_EQ_STR(hk_schema_action_name(HK_SCHEMA_FAIL_SAFE), "fail_safe");
    HK_CHECK_EQ_STR(hk_schema_found_name(HK_SCHEMA_FOUND_NEWER), "newer");
}
