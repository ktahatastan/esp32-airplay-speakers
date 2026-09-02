#include "hk_test.h"
#include "hk_ota.h"

#include <string.h>

/*
 * The gap these tests exist for: ESP-IDF v5.5.1 checks the app descriptor's
 * magic word and nothing else, and never compares project_name anywhere in the
 * OTA path. A correctly built ESP32-S3 image from a different project passes
 * every check IDF performs. So the comparison against the manifest is the only
 * thing standing between a swapped asset and a speaker running foreign
 * firmware, and it is worth testing properly.
 */

static hk_manifest_t manifest(void)
{
    hk_manifest_t m = {0};
    m.present = HK_MANIFEST_REQUIRED_FIELDS;
    strcpy(m.product, "harman-kardom");
    strcpy(m.version, "0.2.0");
    strcpy(m.channel, "stable");
    strcpy(m.target, "esp32s3");
    strcpy(m.hw_revision, "prototype-n16r8");
    strcpy(m.asset, "harman-kardom-0.2.0.bin");
    strcpy(m.sha256, "0123456789abcdef0123456789abcdef"
                     "0123456789abcdef0123456789abcdef");
    strcpy(m.min_updater_version, "0.1.0");
    m.size = 1119232u;
    m.secure_version = 0u;
    return m;
}

static hk_ota_image_t image(void)
{
    hk_ota_image_t i = {0};
    i.valid = true;
    strcpy(i.project_name, "harman-kardom");
    strcpy(i.version, "0.2.0");
    i.secure_version = 0u;
    return i;
}

void test_ota(void)
{
    hk_manifest_t m;
    hk_ota_image_t img;

    /* --- the image the manifest promised --- */
    m = manifest();
    img = image();
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_OK);

    /* --- NULL blocks, it does not pass --- */
    HK_CHECK_EQ_INT(hk_ota_image_check(NULL, &m), HK_OTA_IMAGE_ERR_ARG);
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, NULL), HK_OTA_IMAGE_ERR_ARG);

    /* --- no readable descriptor is a refusal, never a shrug --- */
    img = image();
    img.valid = false;
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_NO_DESC);

    /* An unreadable descriptor must lose even when every other field agrees,
     * because those fields are then uninitialised rather than matching. */
    img = (hk_ota_image_t){0};
    img.valid = false;
    strcpy(img.project_name, "harman-kardom");
    strcpy(img.version, "0.2.0");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_NO_DESC);

    /* --- the case ESP-IDF does not cover: another project's ESP32-S3 image ---
     * Correctly built, correct chip, valid magic word, plausible version. IDF
     * would install it. */
    img = image();
    strcpy(img.project_name, "some-other-project");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_PROJECT);

    /* A near-miss name is still a different project. */
    img = image();
    strcpy(img.project_name, "harman-kardon");   /* n, not m */
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_PROJECT);

    /* A prefix must not pass as a match. */
    img = image();
    strcpy(img.project_name, "harman");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_PROJECT);

    /* --- manifest and image disagreeing on the version --- */
    /* Stale asset: the manifest was updated, the binary was not. */
    img = image();
    strcpy(img.version, "0.1.0");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_VERSION);

    /* A NEWER image is refused too. The two sources disagree; there is no way
     * from here to know which one is wrong, and guessing "newer must be right"
     * is how a mis-uploaded asset installs itself. */
    img = image();
    strcpy(img.version, "0.9.0");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_VERSION);

    /* --- secure_version must agree as well --- */
    img = image();
    img.secure_version = 1u;
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_SECURE_VERSION);

    /* --- checks are ordered, and the first failure is the one reported --- */
    img = image();
    img.valid = false;
    strcpy(img.project_name, "wrong");
    strcpy(img.version, "9.9.9");
    HK_CHECK_EQ_INT(hk_ota_image_check(&img, &m), HK_OTA_IMAGE_ERR_NO_DESC);

    /* --- error names, so a log line is readable --- */
    HK_CHECK(strcmp(hk_ota_image_err_name(HK_OTA_IMAGE_OK), "OK") == 0);
    HK_CHECK(strcmp(hk_ota_image_err_name(HK_OTA_IMAGE_ERR_PROJECT), "PROJECT") == 0);
    HK_CHECK(strcmp(hk_ota_image_err_name(HK_OTA_IMAGE_ERR_NO_DESC), "NO_DESC") == 0);
    HK_CHECK(strcmp(hk_ota_image_err_name((hk_ota_image_err_t)99), "UNKNOWN") == 0);

    /* ===== the address this firmware actually ships with =====
     * Pinned so a typo in it cannot reach a device. A URL the validator
     * refuses would leave every speaker checking nothing, silently, and a URL
     * too long for the buffer would arrive as a missing field. */
    {
        static const char k_shipped[] =
            "https://github.com/ktahatastan/esp32-airplay-speakers"
            "/releases/latest/download/manifest.json";
        HK_CHECK(hk_ota_asset_url_ok(k_shipped));
        HK_CHECK(sizeof(k_shipped) <= HK_MANIFEST_ASSET_MAX);
    }

    /* ================= asset URL ================= */

    /* --- what a real GitHub release link looks like --- */
    HK_CHECK(hk_ota_asset_url_ok(
        "https://github.com/user/repo/releases/download/v0.2.0/harman-kardom.bin"));
    /* And where it redirects to. */
    HK_CHECK(hk_ota_asset_url_ok(
        "https://release-assets.githubusercontent.com/github-production-release-asset/1/2"));
    HK_CHECK(hk_ota_asset_url_ok("https://github.com"));

    /* --- plain http never --- */
    HK_CHECK(!hk_ota_asset_url_ok(
        "http://github.com/user/repo/releases/download/v0.2.0/hk.bin"));
    /* A scheme that is exactly as long as "https://" would leave the host at
     * the same offset, so this is the case that actually pins the scheme test
     * rather than passing because the arithmetic happens to shift. */
    HK_CHECK(!hk_ota_asset_url_ok("httpx://github.com/x.bin"));
    HK_CHECK(!hk_ota_asset_url_ok("XXXXXXXXgithub.com/x.bin"));

    /* --- a look-alike host must not pass on a suffix match --- */
    HK_CHECK(!hk_ota_asset_url_ok("https://evilgithub.com/x.bin"));
    HK_CHECK(!hk_ota_asset_url_ok("https://notgithubusercontent.com/x.bin"));
    /* but a genuine subdomain must */
    HK_CHECK(hk_ota_asset_url_ok("https://objects.githubusercontent.com/x.bin"));

    /* --- credentials in the URL turn the host into something else ---
     * "https://github.com@evil.example/x" has authority github.com@evil.example
     * and connects to evil.example. Refused outright. */
    HK_CHECK(!hk_ota_asset_url_ok("https://github.com@evil.example/x.bin"));
    HK_CHECK(!hk_ota_asset_url_ok("https://user:pass@github.com/x.bin"));
    /* Userinfo in front of a genuine host. The host here really would be
     * github.com, so nothing else in this function rejects it: only the
     * userinfo rule does. */
    HK_CHECK(!hk_ota_asset_url_ok("https://user@github.com/x.bin"));
    HK_CHECK(!hk_ota_asset_url_ok("https://a@b@github.com/x.bin"));

    /* --- a port is not expected on a release link --- */
    HK_CHECK(!hk_ota_asset_url_ok("https://github.com:8443/x.bin"));
    /* Even the default port written out explicitly. */
    HK_CHECK(!hk_ota_asset_url_ok("https://github.com:443/x.bin"));

    /* --- control characters could smuggle a second request line --- */
    HK_CHECK(!hk_ota_asset_url_ok("https://github.com/x.bin\r\nHost: evil"));
    HK_CHECK(!hk_ota_asset_url_ok("https://github.com/x\n.bin"));
    HK_CHECK(!hk_ota_asset_url_ok("https://git hub.com/x.bin"));

    /* --- degenerate input --- */
    HK_CHECK(!hk_ota_asset_url_ok(NULL));
    HK_CHECK(!hk_ota_asset_url_ok(""));
    HK_CHECK(!hk_ota_asset_url_ok("https://"));
    HK_CHECK(!hk_ota_asset_url_ok("https:///path"));
    HK_CHECK(!hk_ota_asset_url_ok("github.com/x.bin"));

    /* --- the query and fragment are not part of the host --- */
    HK_CHECK(hk_ota_asset_url_ok("https://github.com/x.bin?token=abc"));
    HK_CHECK(!hk_ota_asset_url_ok("https://evil.example/x?host=github.com"));
    /* The authority ends at the first '/', '?' or '#'. Without the last two a
     * host could be smuggled past the allow-list in a query or a fragment. */
    HK_CHECK(!hk_ota_asset_url_ok("https://evil.example?.github.com/x"));
    HK_CHECK(!hk_ota_asset_url_ok("https://evil.example#.github.com/x"));
    HK_CHECK(hk_ota_asset_url_ok("https://github.com?a=1"));
    HK_CHECK(hk_ota_asset_url_ok("https://github.com#frag"));
}
