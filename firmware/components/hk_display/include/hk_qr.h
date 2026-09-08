/**
 * @file hk_qr.h
 * @brief A QR encoder, because the code IS the pairing.
 *
 * ADR-0014: the provisioning QR carries the SRP6a username, so a phone that
 * scans it pairs with a device whose advertised name the app would otherwise
 * filter out of its own list. That payload is around 110 bytes of JSON, which
 * measures as version 7 at error correction M -- this comment said version 6
 * until someone encoded it and counted. The Wi-Fi payload is shorter. Versions
 * up to 10 are supported and nothing here allocates.
 *
 * Byte mode only. Alphanumeric mode would pack the Wi-Fi string tighter, but
 * the JSON payload has lowercase letters in it and would fall back to byte mode
 * anyway, and one code path that always works beats two that mostly do.
 */
#ifndef HK_QR_H
#define HK_QR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Version 10 is 57x57 modules. */
#define HK_QR_MAX_VERSION 10
#define HK_QR_MAX_SIZE    (17 + 4 * HK_QR_MAX_VERSION)
#define HK_QR_BUFFER_BYTES ((HK_QR_MAX_SIZE * HK_QR_MAX_SIZE + 7) / 8)

typedef enum {
    HK_QR_ECC_LOW = 0,
    HK_QR_ECC_MEDIUM,
    HK_QR_ECC_QUARTILE,
    HK_QR_ECC_HIGH,
} hk_qr_ecc_t;

typedef struct {
    uint8_t size;                        /**< modules per side */
    uint8_t modules[HK_QR_BUFFER_BYTES]; /**< row-major bitset, 1 = dark */
} hk_qr_t;

/**
 * Encode `text` at the smallest version that fits.
 *
 * Returns false rather than truncating: a QR code that encodes most of a
 * pairing payload is a code that fails after the user has already pointed a
 * camera at it, which is worse than a screen that says it cannot.
 */
bool hk_qr_encode(const char *text, hk_qr_ecc_t ecc, hk_qr_t *out);

/** Encode explicit bytes, for payloads that are not NUL-terminated text. */
bool hk_qr_encode_bytes(const uint8_t *data, size_t length, hk_qr_ecc_t ecc,
                        hk_qr_t *out);

/** Whether the module at (x, y) is dark. Out of range reads as light. */
bool hk_qr_module(const hk_qr_t *qr, int x, int y);

#endif /* HK_QR_H */
