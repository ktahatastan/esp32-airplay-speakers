/**
 * @file hk_version.h
 * @brief SemVer parsing and comparison for the OTA update gate.
 *
 * ADR-0008 states that a normal device only moves to a published, non-prerelease
 * release whose version is strictly greater than its own. This module is the
 * part of that rule that can be decided without a network, so it is pure C and
 * fully covered by host tests.
 *
 * Deliberately strict. An update client that accepts a version it does not
 * fully understand is how a device ends up installing the wrong image, so
 * anything outside plain MAJOR.MINOR.PATCH is rejected rather than guessed at:
 *
 *   - a "v" prefix is accepted, because Git tags carry it (vX.Y.Z)
 *   - prerelease ("1.2.3-rc1") is rejected: it is a canary channel artefact and
 *     must not reach a normal device
 *   - build metadata ("1.2.3+abc") is rejected for the same reason
 *   - leading zeros ("1.02.3") are rejected: SemVer forbids them, and accepting
 *     them would make two spellings of one version compare unequal as strings
 */
#ifndef HK_VERSION_H
#define HK_VERSION_H

#include <stdint.h>

/** A parsed MAJOR.MINOR.PATCH triple. */
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} hk_version_t;

/** Longest accepted version string, including an optional "v" and terminator. */
#define HK_VERSION_MAX_TEXT 34

/** Result of ::hk_version_parse. */
typedef enum {
    HK_VERSION_OK = 0,
    HK_VERSION_ERR_ARG = -1,      /**< NULL argument */
    HK_VERSION_ERR_FORMAT = -2,   /**< Not MAJOR.MINOR.PATCH, or has a prerelease/build suffix */
    HK_VERSION_ERR_RANGE = -3,    /**< A component does not fit in uint32_t */
} hk_version_err_t;

/**
 * Parse a strict SemVer triple, with an optional leading "v".
 *
 * @param text  NUL-terminated version string
 * @param out   filled in on success, untouched otherwise
 * @return HK_VERSION_OK, or a negative ::hk_version_err_t
 */
int hk_version_parse(const char *text, hk_version_t *out);

/**
 * Order two parsed versions.
 *
 * @return -1 when a < b, 0 when equal, 1 when a > b.
 */
int hk_version_compare(const hk_version_t *a, const hk_version_t *b);

/**
 * Decide whether an offered release should be installed.
 *
 * @param offered  version from the release manifest
 * @param running  version currently on the device
 * @return 1 to update, 0 to stay. A string that does not parse returns 0: an
 *         unreadable version is never a reason to flash.
 */
int hk_version_should_update(const char *offered, const char *running);

#endif /* HK_VERSION_H */
