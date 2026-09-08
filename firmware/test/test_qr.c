/**
 * @file test_qr.c
 * @brief The QR encoder: structure here, decodability elsewhere.
 *
 * A QR code is the one thing on this screen whose failure the owner discovers
 * only after raising a camera, so "it looks like a QR code" is not evidence.
 * Two different things are checked, in two different places, because they need
 * different tools.
 *
 * What a host test can prove is structure, and that is what this file does:
 * finder patterns in three corners, their separators, the timing runs, refusal
 * rather than truncation, and determinism.
 *
 * What it cannot prove is that a decoder reads it. That was established
 * separately, against OpenCV's QRCodeDetector, on the real payloads:
 *
 *   HELLO                                              DECODED OK
 *   WIFI:T:WPA;S:HarmanKardom-Setup-932C;P:...;;       DECODED OK
 *   {"ver":"v1","name":"HarmanKardom-932C",...}        DECODED OK   (v6)
 *   WIFI:...;P:4719;;                                  DECODED OK
 *
 * Comparing module-for-module against a second encoder was tried first and is
 * the wrong test: segno produces the same versions and sizes but a different
 * mask, and both codes are conforming. The mask is chosen by a penalty search
 * the standard leaves room in, so two correct encoders may disagree on it and
 * a bit-for-bit digest would fail on a difference that does not exist.
 *
 * The digests below are therefore OUR OWN output, frozen. They prove nothing
 * about correctness -- they catch a change, which is a different and still
 * useful thing, and the decode above is what says the frozen values are right.
 */
#include "hk_test.h"

#include <stdint.h>
#include <string.h>

#include "hk_qr.h"

/* FNV-1a over the packed modules. A digest rather than the matrices
 * themselves: three matrices is 3 KB of literals nobody would ever read, and a
 * mismatch is a mismatch either way. */
static uint64_t digest(const hk_qr_t *qr)
{
    const int bytes = ((int)qr->size * (int)qr->size + 7) / 8;
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < bytes; i++) {
        h = (h ^ qr->modules[i]) * 1099511628211ull;
    }
    return h;
}

/* Frozen output of this encoder, for the payloads decoded above. */
static const struct {
    const char *text;
    uint8_t     size;
    uint64_t    digest;
} VECTORS[] = {
    {"HELLO", 21, 0xae57b4c30a89892cull},
    {"WIFI:T:WPA;S:HarmanKardom-Setup-932C;P:K7QM3XZ29TFB;;",
     33, 0xf075953e33426f71ull},
    {"{\"ver\":\"v1\",\"name\":\"HarmanKardom-932C\",\"username\":\"wifiprov\","
     "\"pop\":\"K7QM3XZ29TFB\",\"transport\":\"ble\"}",
     41, 0xc5c0882bf7329befull},
};

void test_qr(void)
{
    hk_qr_t qr;

    /* Size is the coarse check and it is the one that catches a wrong version
     * or a wrong mode straight away. */
    for (size_t i = 0; i < sizeof(VECTORS) / sizeof(VECTORS[0]); i++) {
        HK_CHECK(hk_qr_encode(VECTORS[i].text, HK_QR_ECC_MEDIUM, &qr));
        HK_CHECK_EQ_INT(qr.size, VECTORS[i].size);
        HK_CHECK(digest(&qr) == VECTORS[i].digest);
    }

    /* The three finder patterns. Every conforming code has them in the same
     * three corners, so their absence means the matrix was never laid out. */
    HK_CHECK(hk_qr_encode("HELLO", HK_QR_ECC_MEDIUM, &qr));
    const int last = (int)qr.size - 1;
    HK_CHECK(hk_qr_module(&qr, 0, 0) && hk_qr_module(&qr, 6, 6));
    HK_CHECK(hk_qr_module(&qr, last, 0) && hk_qr_module(&qr, last - 6, 6));
    HK_CHECK(hk_qr_module(&qr, 0, last) && hk_qr_module(&qr, 6, last - 6));
    /* The separator ring inside each finder is light. */
    HK_CHECK(!hk_qr_module(&qr, 1, 1));
    /* The timing pattern alternates along row and column six. */
    for (int x = 8; x < last - 7; x++) {
        HK_CHECK_EQ_INT(hk_qr_module(&qr, x, 6) ? 1 : 0, (x % 2 == 0) ? 1 : 0);
    }

    /* Out of range reads light rather than reading memory. The draw loop walks
     * a quiet zone past the edge on purpose, so this is load-bearing. */
    HK_CHECK(!hk_qr_module(&qr, -1, 0));
    HK_CHECK(!hk_qr_module(&qr, 0, -1));
    HK_CHECK(!hk_qr_module(&qr, (int)qr.size, 0));
    HK_CHECK(!hk_qr_module(&qr, 0, (int)qr.size));
    HK_CHECK(!hk_qr_module(NULL, 0, 0));

    /* Empty is encodable; a phone gets an empty string rather than nothing. */
    HK_CHECK(hk_qr_encode("", HK_QR_ECC_MEDIUM, &qr));

    /* Refusal, not truncation. A code carrying most of a pairing payload fails
     * after the owner has already raised a camera, which is worse than a screen
     * that says it has nothing to show. */
    char huge[600];
    memset(huge, 'A', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    HK_CHECK(!hk_qr_encode(huge, HK_QR_ECC_HIGH, &qr));
    HK_CHECK(!hk_qr_encode(huge, HK_QR_ECC_LOW, &qr));

    /* Deterministic: the same payload twice is the same code. Mask selection is
     * a search, and a search that is not stable would make a code that scans on
     * one boot and not the next. */
    hk_qr_t again;
    HK_CHECK(hk_qr_encode(VECTORS[2].text, HK_QR_ECC_MEDIUM, &qr));
    HK_CHECK(hk_qr_encode(VECTORS[2].text, HK_QR_ECC_MEDIUM, &again));
    HK_CHECK_EQ_INT(qr.size, again.size);
    HK_CHECK(memcmp(qr.modules, again.modules, sizeof(qr.modules)) == 0);

    /* Higher error correction needs more room, so the same payload must land at
     * the same version or a larger one -- never a smaller one. */
    hk_qr_t low, high;
    HK_CHECK(hk_qr_encode(VECTORS[1].text, HK_QR_ECC_LOW, &low));
    HK_CHECK(hk_qr_encode(VECTORS[1].text, HK_QR_ECC_QUARTILE, &high));
    HK_CHECK(high.size >= low.size);
}
