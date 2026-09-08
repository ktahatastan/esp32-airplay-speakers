/**
 * Parsing the setup form, with no ESP-IDF underneath it.
 *
 * This is the one part of the portal that reads attacker-shaped input: a body
 * posted by whatever joined the setup network. It is kept pure so it can be
 * tested exhaustively on a laptop, years before the radio it protects exists.
 */
#ifndef HK_PORTAL_FORM_H
#define HK_PORTAL_FORM_H

#include <stdbool.h>
#include <stddef.h>

/** 802.11 caps the SSID at 32 bytes; the passphrase at 64 (63 ASCII, or 64 hex). */
#define HK_PORTAL_SSID_MAX 32
#define HK_PORTAL_PASSWORD_MAX 64

typedef struct {
    char ssid[HK_PORTAL_SSID_MAX + 1];
    char password[HK_PORTAL_PASSWORD_MAX + 1];
} hk_portal_credentials_t;

/**
 * Read `ssid` and `password` out of an application/x-www-form-urlencoded body.
 *
 * Returns true only when the body carried a usable SSID. An absent or empty
 * password is accepted, because an open network is a network.
 *
 * The body is NOT required to be NUL-terminated; `length` bounds it. On any
 * rejection `out` is left zeroed rather than half-filled, so a caller that
 * ignores the return value cannot act on a fragment.
 */
bool hk_portal_parse_form(const char *body, size_t length, hk_portal_credentials_t *out);

#endif /* HK_PORTAL_FORM_H */
