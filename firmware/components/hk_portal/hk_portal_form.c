#include "hk_portal_form.h"

#include <string.h>

/** Hex digit to value, or -1. Deliberately not isxdigit(): locale plays no part. */
static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * Percent-decode one field into a bounded buffer.
 *
 * Returns false if the value does not fit or the escaping is malformed. Both
 * are rejections rather than repairs: a truncated SSID silently joins the wrong
 * network, and a "%" the sender did not mean as an escape means the two ends
 * disagree about what was typed.
 */
static bool decode_value(const char *value, size_t length, char *out, size_t capacity)
{
    size_t written = 0;

    for (size_t i = 0; i < length; i++) {
        char decoded;

        if (value[i] == '+') {
            decoded = ' ';
        } else if (value[i] == '%') {
            if (i + 2 >= length) {
                return false;
            }
            const int high = hex_value(value[i + 1]);
            const int low = hex_value(value[i + 2]);
            if (high < 0 || low < 0) {
                return false;
            }
            decoded = (char)((unsigned)high * 16u + (unsigned)low);
            i += 2;
        } else {
            decoded = value[i];
        }

        /* A NUL in the middle would end the string early for every reader after
         * this one, so the two halves would disagree about the value. */
        if (decoded == '\0') {
            return false;
        }
        if (written + 1 >= capacity) {
            return false;
        }
        out[written++] = decoded;
    }

    out[written] = '\0';
    return true;
}

bool hk_portal_parse_form(const char *body, size_t length, hk_portal_credentials_t *out)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (body == NULL) {
        return false;
    }

    bool have_ssid = false;
    bool have_password = false;
    size_t at = 0;

    while (at < length) {
        /* One field: everything up to the next '&'. */
        size_t end = at;
        while (end < length && body[end] != '&') {
            end++;
        }

        size_t separator = at;
        while (separator < end && body[separator] != '=') {
            separator++;
        }

        const char  *key     = body + at;
        const size_t key_len = separator - at;
        /* separator == end means the field had no '=' at all; that is an empty
         * value, not a missing one. */
        const char  *value     = (separator < end) ? body + separator + 1 : body + end;
        const size_t value_len = (separator < end) ? end - separator - 1 : 0;

        if (key_len == 4 && memcmp(key, "ssid", 4) == 0) {
            /* A repeated key is refused rather than resolved. Whichever rule we
             * picked -- first wins, last wins -- would be a rule the sender does
             * not necessarily share, and the disagreement would be about which
             * network the speaker joins. */
            if (have_ssid || !decode_value(value, value_len, out->ssid, sizeof(out->ssid))) {
                memset(out, 0, sizeof(*out));
                return false;
            }
            have_ssid = true;
        } else if (key_len == 8 && memcmp(key, "password", 8) == 0) {
            if (have_password ||
                !decode_value(value, value_len, out->password, sizeof(out->password))) {
                memset(out, 0, sizeof(*out));
                return false;
            }
            have_password = true;
        }
        /* Anything else is ignored: browsers add fields we did not ask for. */

        at = end + 1;
    }

    if (!have_ssid || out->ssid[0] == '\0') {
        memset(out, 0, sizeof(*out));
        return false;
    }
    return true;
}
