#include "hk_test.h"
#include "hk_manifest.h"

#include <string.h>

/*
 * ADR-0008 and the manifest contract in docs/03-firmware/ota-and-release-plan.md.
 *
 * Almost every test here is a refusal. An update client that installs an image
 * it could not fully check is how four speakers brick at the same moment, which
 * is the exact failure the two-slot layout and the canary rollout exist to
 * prevent.
 */

static const char VALID_SHA[] =
    "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";

static hk_device_t device(void)
{
    hk_device_t d = {
        .product = "Harman Kardom",
        .target = "esp32s3",
        .hw_revision = "prototype-n16r8",
        .channel = "stable",
        .running_version = "1.0.0",
        .running_secure_version = 0,
        .slot_size = 0x6e0000,
    };
    return d;
}

static hk_manifest_t good(void)
{
    hk_manifest_t m;
    memset(&m, 0, sizeof(m));
    m.present = HK_MANIFEST_REQUIRED_FIELDS;
    strcpy(m.product, "Harman Kardom");
    strcpy(m.version, "1.0.1");
    strcpy(m.channel, "stable");
    strcpy(m.target, "esp32s3");
    strcpy(m.hw_revision, "prototype-n16r8");
    strcpy(m.asset, "harman-kardom-esp32s3-n16r8-v1.0.1.bin");
    strcpy(m.sha256, VALID_SHA);
    strcpy(m.min_updater_version, "0.1.0");
    m.size = 1200000;
    m.secure_version = 0;
    return m;
}

void test_manifest(void)
{
    /* --- an actual release URL has to fit ---
     * The buffer was 96 bytes once, and the real URL for this repository is
     * 97. Nothing failed at build time; every device would simply have refused
     * every update as "field missing". Pinned here with the longest shape the
     * project can realistically produce. */
    {
        static const char k_real_url[] =
            "https://github.com/serbaysancak/esp32-airplay-speakers"
            "/releases/download/v10.20.30/harman-kardom.bin";
        HK_CHECK(sizeof(k_real_url) <= HK_MANIFEST_ASSET_MAX);

        /* And the redirect target GitHub actually serves it from, which is
         * longer still. */
        static const char k_redirect[] =
            "https://release-assets.githubusercontent.com"
            "/github-production-release-asset/123456789/abcdef01-2345-6789-abcd-ef0123456789";
        HK_CHECK(sizeof(k_redirect) <= HK_MANIFEST_ASSET_MAX);
    }

    hk_device_t dev = device();
    hk_manifest_t m;

    /* --- the release this device is waiting for --- */
    m = good();
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);

    /* --- every field is required --- */
    m = good();
    m.present &= ~(uint32_t)HK_MF_SHA256;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_FIELD_MISSING);
    m = good();
    m.present &= ~(uint32_t)HK_MF_HW_REVISION;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_FIELD_MISSING);
    m = good();
    m.present = 0;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_FIELD_MISSING);

    /* --- built for something else --- */
    m = good();
    strcpy(m.product, "Other Speaker");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_PRODUCT);
    m = good();
    strcpy(m.target, "esp32c3");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_TARGET);
    /* The N8R8 variant: same chip, half the flash, and an image that assumes a
     * 16 MB layout would not fit its slots. */
    m = good();
    strcpy(m.hw_revision, "prototype-n8r8");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_HW_REVISION);

    /* A prefix must not pass as a match, in either direction. */
    m = good();
    strcpy(m.target, "esp32");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_TARGET);
    m = good();
    strcpy(m.product, "Harman Kardom Pro");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_PRODUCT);

    /* --- a stable device does not take a canary build --- */
    m = good();
    strcpy(m.channel, "canary");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_CHANNEL);

    /* --- versions --- */
    m = good();
    strcpy(m.version, "1.0.1-rc1");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_VERSION);
    m = good();
    strcpy(m.version, "not-a-version");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_VERSION);
    m = good();
    strcpy(m.version, "1.0.0");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_NOT_NEWER);
    m = good();
    strcpy(m.version, "0.9.9");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_NOT_NEWER);
    m = good();
    strcpy(m.version, "v1.0.1");   /* a Git tag spelling is still acceptable */
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);

    /* --- the manifest may demand newer firmware than this --- */
    m = good();
    strcpy(m.min_updater_version, "2.0.0");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_UPDATER_TOO_OLD);
    m = good();
    strcpy(m.min_updater_version, "1.0.0");  /* exactly what is running is enough */
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);

    /* --- the digest --- */
    m = good();
    strcpy(m.sha256, "9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08");
    /* Uppercase is refused rather than folded: this value is compared against
     * one the device computes, and two spellings invite an accidental match. */
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SHA256);
    m = good();
    strcpy(m.sha256, "9f86d081");
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SHA256);
    m = good();
    m.sha256[0] = 'z';
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SHA256);
    m = good();
    m.sha256[0] = '\0';
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SHA256);

    /* --- size, checked before a byte is fetched --- */
    m = good();
    m.size = 0;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SIZE);
    m = good();
    m.size = dev.slot_size + 1u;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_SIZE);
    m = good();
    m.size = dev.slot_size;  /* exactly filling the slot is allowed */
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);

    /* --- anti-rollback --- */
    dev.running_secure_version = 3;
    m = good();
    m.secure_version = 2;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_ERR_ROLLBACK);
    m = good();
    m.secure_version = 3;  /* the same is fine; only going backwards is not */
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);
    m = good();
    m.secure_version = 4;
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, &dev), HK_MANIFEST_OK);
    dev.running_secure_version = 0;

    /* --- arguments --- */
    m = good();
    HK_CHECK_EQ_INT(hk_manifest_validate(NULL, &dev), HK_MANIFEST_ERR_ARG);
    HK_CHECK_EQ_INT(hk_manifest_validate(&m, NULL), HK_MANIFEST_ERR_ARG);

    HK_CHECK_EQ_STR(hk_manifest_err_name(HK_MANIFEST_ERR_ROLLBACK), "secure_version_rollback");
}
