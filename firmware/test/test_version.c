#include "hk_test.h"
#include "hk_version.h"

/*
 * ADR-0008: a normal device only installs a published, non-prerelease release
 * that is strictly newer than the one it runs. Every rejection below is a case
 * where a looser parser would have flashed the wrong image.
 */
void test_version(void)
{
    hk_version_t v;

    HK_CHECK_EQ_INT(hk_version_parse("1.2.3", &v), HK_VERSION_OK);
    HK_CHECK_EQ_INT(v.major, 1);
    HK_CHECK_EQ_INT(v.minor, 2);
    HK_CHECK_EQ_INT(v.patch, 3);

    /* Git tags carry a v prefix. */
    HK_CHECK_EQ_INT(hk_version_parse("v0.1.0", &v), HK_VERSION_OK);
    HK_CHECK_EQ_INT(v.major, 0);
    HK_CHECK_EQ_INT(v.minor, 1);
    HK_CHECK_EQ_INT(v.patch, 0);

    HK_CHECK_EQ_INT(hk_version_parse("10.20.30", &v), HK_VERSION_OK);
    HK_CHECK_EQ_INT(v.minor, 20);

    /* Prerelease and build metadata belong to the canary channel only. */
    HK_CHECK_EQ_INT(hk_version_parse("1.2.3-rc1", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("1.2.3+build7", &v), HK_VERSION_ERR_FORMAT);

    /* Malformed input must be rejected, never guessed at. */
    HK_CHECK_EQ_INT(hk_version_parse("1.2", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("1.2.3.4", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("1.02.3", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("v", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("a.b.c", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse(" 1.2.3", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse("1.2.3 ", &v), HK_VERSION_ERR_FORMAT);
    HK_CHECK_EQ_INT(hk_version_parse(NULL, &v), HK_VERSION_ERR_ARG);
    HK_CHECK_EQ_INT(hk_version_parse("1.2.3", NULL), HK_VERSION_ERR_ARG);

    /* A component larger than uint32_t must not wrap into a small number. */
    HK_CHECK_EQ_INT(hk_version_parse("4294967296.0.0", &v), HK_VERSION_ERR_RANGE);
    HK_CHECK_EQ_INT(hk_version_parse("4294967295.0.0", &v), HK_VERSION_OK);

    /* Ordering. */
    hk_version_t a;
    hk_version_t b;
    hk_version_parse("1.2.3", &a);
    hk_version_parse("1.2.4", &b);
    HK_CHECK_EQ_INT(hk_version_compare(&a, &b), -1);
    HK_CHECK_EQ_INT(hk_version_compare(&b, &a), 1);
    HK_CHECK_EQ_INT(hk_version_compare(&a, &a), 0);
    hk_version_parse("1.10.0", &a);
    hk_version_parse("1.9.0", &b);
    HK_CHECK_EQ_INT(hk_version_compare(&a, &b), 1);  /* not a string comparison */

    /* The major term is the highest-order part of the ADR-0008 rule. Without
     * these, a sign flip in the major comparison passes the whole suite. */
    hk_version_parse("2.0.0", &a);
    hk_version_parse("1.9.9", &b);
    HK_CHECK_EQ_INT(hk_version_compare(&a, &b), 1);
    HK_CHECK_EQ_INT(hk_version_compare(&b, &a), -1);
    hk_version_parse("1.0.0", &a);
    hk_version_parse("2.0.0", &b);
    HK_CHECK_EQ_INT(hk_version_compare(&a, &b), -1);

    /* The update decision itself. */
    HK_CHECK_EQ_INT(hk_version_should_update("1.2.4", "1.2.3"), 1);
    HK_CHECK_EQ_INT(hk_version_should_update("v1.2.4", "1.2.3"), 1);
    HK_CHECK_EQ_INT(hk_version_should_update("1.2.3", "1.2.3"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update("1.2.2", "1.2.3"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update("2.0.0-rc1", "1.2.3"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update("garbage", "1.2.3"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update("1.2.4", "garbage"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update(NULL, "1.2.3"), 0);
    HK_CHECK_EQ_INT(hk_version_should_update("2.0.0", "1.9.9"), 1);
    HK_CHECK_EQ_INT(hk_version_should_update("1.9.9", "2.0.0"), 0);

    /* parse_component is shared by all three positions, so overflow is checked
     * in each of them rather than only in the major slot. */
    HK_CHECK_EQ_INT(hk_version_parse("0.4294967296.0", &v), HK_VERSION_ERR_RANGE);
    HK_CHECK_EQ_INT(hk_version_parse("0.0.4294967296", &v), HK_VERSION_ERR_RANGE);
    HK_CHECK_EQ_INT(hk_version_parse("0.4294967295.4294967295", &v), HK_VERSION_OK);

    /* A string longer than the module accepts is rejected, not truncated. */
    HK_CHECK_EQ_INT(hk_version_parse("1.2.33333333333333333333333333333333", &v),
                    HK_VERSION_ERR_FORMAT);
}
