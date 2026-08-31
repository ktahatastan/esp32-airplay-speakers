#include "hk_test.h"
#include "hk_identity.h"

/*
 * Covers the identity table in docs/controls-and-provisioning-plan.md and
 * ADR-0001. Four speakers have to be distinguishable at first boot, so the
 * suffix and its propagation into every surface is the load-bearing part.
 */
void test_identity(void)
{
    const uint8_t mac[6] = {0x24, 0x6F, 0x28, 0x11, 0xA1, 0xB2};
    hk_identity_t id;

    HK_CHECK_EQ_INT(hk_identity_from_mac(mac, &id), HK_IDENTITY_OK);
    HK_CHECK_EQ_STR(id.suffix, "A1B2");
    HK_CHECK_EQ_STR(id.airplay, "Harman Kardom A1B2");
    HK_CHECK_EQ_STR(id.ble, "HarmanKardom-A1B2");
    HK_CHECK_EQ_STR(id.softap, "HarmanKardom-Setup-A1B2");
    HK_CHECK_EQ_STR(id.mdns, "harman-kardom-a1b2");

    /* Low nibbles and zero bytes must still produce four characters. */
    const uint8_t mac_zeros[6] = {0x24, 0x6F, 0x28, 0x11, 0x00, 0x0F};
    HK_CHECK_EQ_INT(hk_identity_from_mac(mac_zeros, &id), HK_IDENTITY_OK);
    HK_CHECK_EQ_STR(id.suffix, "000F");
    HK_CHECK_EQ_STR(id.mdns, "harman-kardom-000f");

    /* Two units differing only in the last octet must not collide. */
    const uint8_t mac_a[6] = {0x24, 0x6F, 0x28, 0x11, 0xA1, 0x01};
    const uint8_t mac_b[6] = {0x24, 0x6F, 0x28, 0x11, 0xA1, 0x02};
    hk_identity_t id_a;
    hk_identity_t id_b;
    HK_CHECK_EQ_INT(hk_identity_from_mac(mac_a, &id_a), HK_IDENTITY_OK);
    HK_CHECK_EQ_INT(hk_identity_from_mac(mac_b, &id_b), HK_IDENTITY_OK);
    HK_CHECK(strcmp(id_a.suffix, id_b.suffix) != 0);
    HK_CHECK(strcmp(id_a.softap, id_b.softap) != 0);

    /* An all-zero MAC means the interface was read too early. Naming four
     * speakers from it would give them all the same name, so it is an error. */
    const uint8_t mac_unset[6] = {0, 0, 0, 0, 0, 0};
    HK_CHECK_EQ_INT(hk_identity_from_mac(mac_unset, &id), HK_IDENTITY_ERR_MAC);
    HK_CHECK_EQ_INT(hk_identity_from_mac(NULL, &id), HK_IDENTITY_ERR_ARG);
    HK_CHECK_EQ_INT(hk_identity_from_mac(mac, NULL), HK_IDENTITY_ERR_ARG);

    /* Protocol limits, restated as runtime checks over a real value. */
    HK_CHECK(strlen(id_a.softap) <= 32);
    HK_CHECK(strlen(id_a.ble) <= 29);
    HK_CHECK(hk_identity_is_valid_mdns_label(id_a.mdns));

    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label("harman-kardom-a1b2"), 1);
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label("Harman-Kardom"), 0);  /* uppercase */
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label("-leading"), 0);
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label("trailing-"), 0);
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label("has_underscore"), 0);
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label(""), 0);
    HK_CHECK_EQ_INT(hk_identity_is_valid_mdns_label(NULL), 0);
}
