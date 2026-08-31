#include "hk_identity.h"

#include <stdio.h>
#include <string.h>

static const char HEX_UPPER[] = "0123456789ABCDEF";

int hk_identity_from_mac(const uint8_t mac[6], hk_identity_t *out)
{
    if (mac == NULL || out == NULL) {
        return HK_IDENTITY_ERR_ARG;
    }

    /* An all-zero MAC means the netif was read before it was initialised.
     * Naming four speakers from that would give them all the same name. */
    uint8_t seen = 0;
    for (int i = 0; i < 6; i++) {
        seen |= mac[i];
    }
    if (seen == 0) {
        return HK_IDENTITY_ERR_MAC;
    }

    char lower[HK_SUFFIX_LEN + 1];
    const uint8_t tail[2] = {mac[4], mac[5]};
    for (int i = 0; i < 2; i++) {
        out->suffix[i * 2]     = HEX_UPPER[(tail[i] >> 4) & 0x0F];
        out->suffix[i * 2 + 1] = HEX_UPPER[tail[i] & 0x0F];
        lower[i * 2]     = (char)(out->suffix[i * 2] | 0x20);
        lower[i * 2 + 1] = (char)(out->suffix[i * 2 + 1] | 0x20);
    }
    out->suffix[HK_SUFFIX_LEN] = '\0';
    lower[HK_SUFFIX_LEN] = '\0';

    /* Buffer sizes are exact, so these cannot truncate; snprintf is used for
     * the guaranteed termination rather than for the bound. */
    snprintf(out->airplay, sizeof(out->airplay), HK_PRODUCT_FAMILY " %s", out->suffix);
    snprintf(out->ble, sizeof(out->ble), "HarmanKardom-%s", out->suffix);
    snprintf(out->softap, sizeof(out->softap), "HarmanKardom-Setup-%s", out->suffix);
    snprintf(out->mdns, sizeof(out->mdns), "harman-kardom-%s", lower);

    return HK_IDENTITY_OK;
}

int hk_identity_is_valid_mdns_label(const char *label)
{
    if (label == NULL) {
        return 0;
    }
    size_t length = strlen(label);
    if (length == 0 || length > 63) {
        return 0;
    }
    if (label[0] == '-' || label[length - 1] == '-') {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        char c = label[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) {
            return 0;
        }
    }
    return 1;
}
