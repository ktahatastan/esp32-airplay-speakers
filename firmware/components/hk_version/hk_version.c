#include "hk_version.h"

#include <stddef.h>
#include <string.h>

/**
 * Read one decimal component and advance the cursor.
 *
 * Rejects an empty component, a leading zero on a multi-digit number, and any
 * value that would overflow uint32_t.
 */
static int parse_component(const char **cursor, uint32_t *value)
{
    const char *p = *cursor;
    if (*p < '0' || *p > '9') {
        return HK_VERSION_ERR_FORMAT;
    }
    if (p[0] == '0' && p[1] >= '0' && p[1] <= '9') {
        return HK_VERSION_ERR_FORMAT;  /* SemVer forbids leading zeros */
    }

    uint32_t accumulated = 0;
    while (*p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if (accumulated > (UINT32_MAX - digit) / 10u) {
            return HK_VERSION_ERR_RANGE;
        }
        accumulated = accumulated * 10u + digit;
        p++;
    }
    *cursor = p;
    *value = accumulated;
    return HK_VERSION_OK;
}

int hk_version_parse(const char *text, hk_version_t *out)
{
    if (text == NULL || out == NULL) {
        return HK_VERSION_ERR_ARG;
    }
    if (strnlen(text, HK_VERSION_MAX_TEXT) >= HK_VERSION_MAX_TEXT) {
        return HK_VERSION_ERR_FORMAT;
    }

    const char *cursor = text;
    if (*cursor == 'v') {
        cursor++;  /* Git tags are vX.Y.Z */
    }

    hk_version_t parsed;
    int status = parse_component(&cursor, &parsed.major);
    if (status != HK_VERSION_OK) {
        return status;
    }
    if (*cursor++ != '.') {
        return HK_VERSION_ERR_FORMAT;
    }
    status = parse_component(&cursor, &parsed.minor);
    if (status != HK_VERSION_OK) {
        return status;
    }
    if (*cursor++ != '.') {
        return HK_VERSION_ERR_FORMAT;
    }
    status = parse_component(&cursor, &parsed.patch);
    if (status != HK_VERSION_OK) {
        return status;
    }

    /* Anything left over is a prerelease tag, build metadata or trailing junk.
     * None of those may reach a normal device (ADR-0008). */
    if (*cursor != '\0') {
        return HK_VERSION_ERR_FORMAT;
    }

    *out = parsed;
    return HK_VERSION_OK;
}

int hk_version_compare(const hk_version_t *a, const hk_version_t *b)
{
    if (a->major != b->major) {
        return a->major < b->major ? -1 : 1;
    }
    if (a->minor != b->minor) {
        return a->minor < b->minor ? -1 : 1;
    }
    if (a->patch != b->patch) {
        return a->patch < b->patch ? -1 : 1;
    }
    return 0;
}

int hk_version_should_update(const char *offered, const char *running)
{
    hk_version_t offered_version;
    hk_version_t running_version;
    if (hk_version_parse(offered, &offered_version) != HK_VERSION_OK) {
        return 0;
    }
    if (hk_version_parse(running, &running_version) != HK_VERSION_OK) {
        return 0;
    }
    return hk_version_compare(&offered_version, &running_version) > 0 ? 1 : 0;
}
