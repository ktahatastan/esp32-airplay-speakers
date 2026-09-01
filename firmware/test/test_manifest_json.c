#include "hk_test.h"
#include "hk_ota.h"

#include <string.h>

/*
 * The parser sits between the release tooling and the validator, and its job
 * is narrower than it looks: record what actually arrived, and never guess.
 * A field that is missing, the wrong type, or too long to hold must end up
 * ABSENT rather than partially present, because the validator's whole
 * approach is to refuse what it cannot fully check — and a truncated product
 * name or a rounded size would arrive looking like something it could.
 */

static const char *k_good =
    "{"
    "\"product\":\"harman-kardom\","
    "\"version\":\"0.2.0\","
    "\"channel\":\"stable\","
    "\"target\":\"esp32s3\","
    "\"hw_revision\":\"prototype-n16r8\","
    "\"asset\":\"https://github.com/o/r/releases/download/v0.2.0/hk.bin\","
    "\"size\":1119232,"
    "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
    "\"secure_version\":0,"
    "\"min_updater_version\":\"0.1.0\""
    "}";

void test_manifest_json(void)
{
    hk_manifest_t m;

    /* ===== a well-formed manifest arrives whole ===== */
    HK_CHECK(hk_manifest_parse(k_good, &m));
    HK_CHECK_EQ_INT(m.present, HK_MANIFEST_REQUIRED_FIELDS);
    HK_CHECK(strcmp(m.product, "harman-kardom") == 0);
    HK_CHECK(strcmp(m.version, "0.2.0") == 0);
    HK_CHECK(strcmp(m.target, "esp32s3") == 0);
    HK_CHECK(strcmp(m.hw_revision, "prototype-n16r8") == 0);
    HK_CHECK(strcmp(m.channel, "stable") == 0);
    HK_CHECK_EQ_INT(m.size, 1119232);
    HK_CHECK_EQ_INT(m.secure_version, 0);
    HK_CHECK(strlen(m.sha256) == 64);

    /* ===== not JSON at all ===== */
    HK_CHECK(!hk_manifest_parse("", &m));
    HK_CHECK(!hk_manifest_parse("{", &m));
    HK_CHECK(!hk_manifest_parse("not json", &m));
    HK_CHECK(!hk_manifest_parse(NULL, &m));

    /* ===== a missing field is absent, and the rest still arrive ===== */
    HK_CHECK(hk_manifest_parse(
        "{\"product\":\"harman-kardom\",\"version\":\"0.2.0\"}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) != 0);
    HK_CHECK((m.present & HK_MF_VERSION) != 0);
    HK_CHECK((m.present & HK_MF_SIZE) == 0);
    HK_CHECK(m.present != HK_MANIFEST_REQUIRED_FIELDS);

    /* ===== the wrong type is not a value ===== */
    HK_CHECK(hk_manifest_parse("{\"product\":42}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) == 0);
    HK_CHECK(hk_manifest_parse("{\"size\":\"1119232\"}", &m));
    HK_CHECK((m.present & HK_MF_SIZE) == 0);
    HK_CHECK(hk_manifest_parse("{\"product\":null}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) == 0);
    HK_CHECK(hk_manifest_parse("{\"product\":[\"harman-kardom\"]}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) == 0);

    /* ===== an empty string is not a value either ===== */
    HK_CHECK(hk_manifest_parse("{\"product\":\"\"}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) == 0);

    /* ===== too long is refused, never truncated =====
     * This is the one that would be dangerous if it went the other way: a
     * product name cut down to fit could match the device's own, and a
     * shortened digest could still be 64 characters of something. */
    {
        char json[512];
        (void)snprintf(json, sizeof(json),
                       "{\"product\":\"%s\"}",
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        HK_CHECK(hk_manifest_parse(json, &m));
        HK_CHECK((m.present & HK_MF_PRODUCT) == 0);
        HK_CHECK(m.product[0] == '\0');
    }

    /* A string of exactly the buffer size minus one still fits. */
    {
        char json[128];
        char channel[HK_MANIFEST_CHANNEL_MAX];
        memset(channel, 'b', sizeof(channel) - 1u);
        channel[sizeof(channel) - 1u] = '\0';
        (void)snprintf(json, sizeof(json), "{\"channel\":\"%s\"}", channel);
        HK_CHECK(hk_manifest_parse(json, &m));
        HK_CHECK((m.present & HK_MF_CHANNEL) != 0);
        HK_CHECK(strlen(m.channel) == sizeof(channel) - 1u);
    }

    /* ===== numbers that are not counts ===== */
    HK_CHECK(hk_manifest_parse("{\"size\":-1}", &m));
    HK_CHECK((m.present & HK_MF_SIZE) == 0);
    HK_CHECK(hk_manifest_parse("{\"size\":1119232.5}", &m));
    HK_CHECK((m.present & HK_MF_SIZE) == 0);   /* rounding would invent a size */
    HK_CHECK(hk_manifest_parse("{\"size\":99999999999}", &m));
    HK_CHECK((m.present & HK_MF_SIZE) == 0);   /* beyond uint32 */
    HK_CHECK(hk_manifest_parse("{\"size\":0}", &m));
    HK_CHECK((m.present & HK_MF_SIZE) != 0);   /* zero is a value; the validator judges it */

    /* ===== a real asset URL fits ===== */
    HK_CHECK(hk_manifest_parse(k_good, &m));
    HK_CHECK(strlen(m.asset) > 40);
    HK_CHECK(hk_ota_asset_url_ok(m.asset));

    /* ===== the parser leaves nothing from a previous call behind ===== */
    HK_CHECK(hk_manifest_parse(k_good, &m));
    HK_CHECK(hk_manifest_parse("{\"version\":\"9.9.9\"}", &m));
    HK_CHECK(m.product[0] == '\0');
    HK_CHECK_EQ_INT(m.size, 0);
    HK_CHECK_EQ_INT(m.present, HK_MF_VERSION);

    /* ===== unknown fields are ignored rather than fatal ===== */
    HK_CHECK(hk_manifest_parse(
        "{\"product\":\"harman-kardom\",\"future_field\":{\"a\":1}}", &m));
    HK_CHECK((m.present & HK_MF_PRODUCT) != 0);

    /* ===== a NULL destination must not be written through ===== */
    HK_CHECK(!hk_manifest_parse(k_good, NULL));
}
