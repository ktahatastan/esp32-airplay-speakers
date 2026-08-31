#include "hk_ota.h"

#include <stddef.h>
#include <string.h>

/**
 * Hosts a release asset may be served from.
 *
 * Matched as a suffix on a domain boundary, so "evilgithub.com" does not pass
 * and "release-assets.githubusercontent.com" does. Kept short deliberately: a
 * long allow-list is a long list of things that can go wrong, and ADR-0008
 * names exactly one distribution source.
 */
static const char *const k_allowed_hosts[] = {
    "github.com",
    "githubusercontent.com",
};

hk_ota_image_err_t hk_ota_image_check(const hk_ota_image_t *image,
                                      const hk_manifest_t *manifest)
{
    if (image == NULL || manifest == NULL) {
        return HK_OTA_IMAGE_ERR_ARG;
    }
    if (!image->valid) {
        return HK_OTA_IMAGE_ERR_NO_DESC;
    }
    if (strcmp(image->project_name, manifest->product) != 0) {
        return HK_OTA_IMAGE_ERR_PROJECT;
    }
    if (strcmp(image->version, manifest->version) != 0) {
        return HK_OTA_IMAGE_ERR_VERSION;
    }
    if (image->secure_version != manifest->secure_version) {
        return HK_OTA_IMAGE_ERR_SECURE_VERSION;
    }
    return HK_OTA_IMAGE_OK;
}

const char *hk_ota_image_err_name(hk_ota_image_err_t error)
{
    switch (error) {
    case HK_OTA_IMAGE_OK:                 return "OK";
    case HK_OTA_IMAGE_ERR_ARG:            return "ARG";
    case HK_OTA_IMAGE_ERR_NO_DESC:        return "NO_DESC";
    case HK_OTA_IMAGE_ERR_PROJECT:        return "PROJECT";
    case HK_OTA_IMAGE_ERR_VERSION:        return "VERSION";
    case HK_OTA_IMAGE_ERR_SECURE_VERSION: return "SECURE_VERSION";
    }
    return "UNKNOWN";
}

/** True if @p host is, or is a subdomain of, @p domain. */
static bool host_matches(const char *host, size_t host_len, const char *domain)
{
    const size_t domain_len = strlen(domain);

    if (host_len < domain_len) {
        return false;
    }
    if (memcmp(host + host_len - domain_len, domain, domain_len) != 0) {
        return false;
    }
    /* Exact match, or a real label boundary: ".github.com" but not "xgithub.com". */
    return host_len == domain_len || host[host_len - domain_len - 1u] == '.';
}

bool hk_ota_asset_url_ok(const char *url)
{
    static const char k_scheme[] = "https://";

    if (url == NULL) {
        return false;
    }

    /* Nothing outside the printable ASCII range, anywhere. A newline in a URL
     * is how a second header line gets smuggled into the request. */
    for (const char *p = url; *p != '\0'; p++) {
        const unsigned char c = (unsigned char)*p;
        if (c <= 0x20u || c >= 0x7fu) {
            return false;
        }
    }

    if (strncmp(url, k_scheme, sizeof(k_scheme) - 1u) != 0) {
        return false;
    }

    /* The authority runs from after the scheme to the first '/', '?' or '#'. */
    const char *authority = url + sizeof(k_scheme) - 1u;
    size_t authority_len = 0;
    while (authority[authority_len] != '\0' && authority[authority_len] != '/' &&
           authority[authority_len] != '?' && authority[authority_len] != '#') {
        authority_len++;
    }
    if (authority_len == 0u) {
        return false;
    }

    /* Split the authority the way a client does, so each rule below is judged
     * against the host that would actually be connected to. Deciding on the
     * raw authority instead would let a malformed one fail the allow-list by
     * accident, which looks like a working check and is not one. */
    size_t userinfo_len = 0;   /* including the '@' */
    for (size_t i = 0; i < authority_len; i++) {
        if (authority[i] == '@') {
            userinfo_len = i + 1u;   /* last '@' wins, as in RFC 3986 */
        }
    }
    const char *host = authority + userinfo_len;
    size_t host_len = authority_len - userinfo_len;

    size_t port_at = host_len;
    for (size_t i = 0; i < host_len; i++) {
        if (host[i] == ':') {
            port_at = i;
            break;
        }
    }
    const size_t bare_host_len = port_at;

    /* Credentials in a release link are never legitimate, and they are the
     * classic way to make a URL read as one host while resolving to another. */
    if (userinfo_len != 0u) {
        return false;
    }
    /* Likewise a port: GitHub serves releases on 443 and nothing else. */
    if (port_at != host_len) {
        return false;
    }
    if (bare_host_len == 0u) {
        return false;
    }

    for (size_t i = 0; i < sizeof(k_allowed_hosts) / sizeof(k_allowed_hosts[0]); i++) {
        if (host_matches(host, bare_host_len, k_allowed_hosts[i])) {
            return true;
        }
    }
    return false;
}
