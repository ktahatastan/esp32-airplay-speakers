/**
 * @file hk_identity.h
 * @brief Device naming for every user-visible surface.
 *
 * Implements the identity table in docs/controls-and-provisioning-plan.md and
 * ADR-0001. Every name carries a short suffix derived from the device MAC so
 * the device is unique on any network it joins — beside another board running
 * this firmware (the bench devkit, a replacement board) or anyone else's
 * device.
 *
 * Pure C: no ESP-IDF dependency, so the naming rules can be tested on the host.
 */
#ifndef HK_IDENTITY_H
#define HK_IDENTITY_H

#include <stdint.h>

/** Product family name. The user may rename the AirPlay surface; this stays. */
#define HK_PRODUCT_FAMILY "Merzarkabul Airplay Speakers"

/*
 * The short name every per-device surface is built from. The family name is
 * the boot banner and the label; it is too long to lead an SSID or a BLE
 * advertisement once a suffix is appended, so the surfaces use this instead.
 * Each spelling is defined exactly once and the size macros below are derived
 * from the same literal the formatter prints, so a rename cannot leave a
 * buffer one byte short of the string that goes into it.
 *
 * The two setup surfaces -- the BLE advertisement and the setup network's SSID
 * -- share ONE name, and it starts with "PROV_" (ADR-0023). The prefix is not
 * decoration: Espressif's stock provisioning apps filter the device list by
 * that prefix by default (Android app/build.gradle: ble_device_name_prefix and
 * wifi_device_name_prefix "PROV_"; iOS Example Utility.swift: deviceNamePrefix
 * "PROV_"), so a device named any other way is not listed until the user finds
 * the setting and changes it. Carrying the prefix is what lets the speaker be
 * picked from the list with no QR and no app configuration -- the whole point
 * of removing the PIN. One name rather than two because the user should see
 * the same string on the phone whichever transport it found the speaker on.
 */
#define HK_NAME_PREFIX        "Merzarkabul"
#define HK_NAME_PREFIX_SETUP  "PROV_" HK_NAME_PREFIX "-"
#define HK_NAME_PREFIX_BLE    HK_NAME_PREFIX_SETUP
#define HK_NAME_PREFIX_SOFTAP HK_NAME_PREFIX_SETUP
#define HK_NAME_PREFIX_MDNS   "merzarkabul-"

/**
 * Hardware revision. The product board is N16R8 (ADR-0010).
 *
 * The manifest carries the revision a release was built for and the device
 * refuses anything that does not match, so this string and the one
 * make_manifest.py writes have to agree exactly. Defined once here rather than
 * typed at each use: two spellings would produce a device that refuses every
 * release with a message about hardware, which reads like a hardware problem.
 *
 * The bring-up devkit overrides it from components/hk_identity/CMakeLists.txt,
 * because a devkit and a product board must never accept each other's releases:
 * they have different flash sizes and different partition tables, so an image
 * built for one does not merely misbehave on the other, it lands in the wrong
 * place. The override is driven by the Kconfig board option rather than a
 * command-line -D, so that selecting the board selects the revision with it.
 *
 * The default is the product value, which leaves the host tests and every build
 * that does not deliberately ask for the devkit unchanged.
 */
#ifndef HK_HW_REVISION
#define HK_HW_REVISION "prototype-n16r8"
#endif

/** Captive portal page title. */
#define HK_PORTAL_TITLE "Merzarkabul Kurulum"

/** Suffix length in characters, excluding the terminator. */
#define HK_SUFFIX_LEN 4

/*
 * Buffer sizes are the exact worst case: prefix + suffix + terminator. They are
 * checked against the protocol limits by static assertions below, so a future
 * rename cannot silently produce an SSID the radio will truncate.
 */
#define HK_NAME_AIRPLAY_SIZE (sizeof(HK_NAME_PREFIX) + 1 + HK_SUFFIX_LEN)
#define HK_NAME_BLE_SIZE     (sizeof(HK_NAME_PREFIX_BLE) + HK_SUFFIX_LEN)
#define HK_NAME_SOFTAP_SIZE  (sizeof(HK_NAME_PREFIX_SOFTAP) + HK_SUFFIX_LEN)
#define HK_NAME_MDNS_SIZE    (sizeof(HK_NAME_PREFIX_MDNS) + HK_SUFFIX_LEN)

/** IEEE 802.11 caps an SSID at 32 octets. */
_Static_assert(HK_NAME_SOFTAP_SIZE - 1 <= 32, "SoftAP SSID would be truncated");
/**
 * A BLE legacy advertising payload is 31 octets; the complete-local-name AD
 * structure costs 2 of them, so the name itself has 29 to work with.
 */
_Static_assert(HK_NAME_BLE_SIZE - 1 <= 29, "BLE local name would not fit one advertising packet");
/** A single DNS label is limited to 63 octets. */
_Static_assert(HK_NAME_MDNS_SIZE - 1 <= 63, "mDNS host label exceeds one DNS label");

/**
 * Every name this device answers to.
 *
 * `ble` and `softap` hold the same string by construction (see
 * HK_NAME_PREFIX_SETUP). They stay two fields because they are two surfaces
 * -- a BLE advertisement and an SSID -- with two protocol limits, two callers
 * and two lines in the boot report; a reader of either should not have to know
 * that the other one happens to be spelled the same way today.
 */
typedef struct {
    char suffix[HK_SUFFIX_LEN + 1];        /**< Uppercase, e.g. "A1B2" */
    char airplay[HK_NAME_AIRPLAY_SIZE];    /**< "Merzarkabul A1B2" */
    char ble[HK_NAME_BLE_SIZE];            /**< "PROV_Merzarkabul-A1B2" */
    char softap[HK_NAME_SOFTAP_SIZE];      /**< "PROV_Merzarkabul-A1B2", the setup SSID */
    char mdns[HK_NAME_MDNS_SIZE];          /**< "merzarkabul-a1b2", lowercase */
} hk_identity_t;

/** Result of ::hk_identity_from_mac. */
typedef enum {
    HK_IDENTITY_OK = 0,
    HK_IDENTITY_ERR_ARG = -1,      /**< NULL argument */
    HK_IDENTITY_ERR_MAC = -2,      /**< All-zero MAC: the interface is not initialised */
} hk_identity_err_t;

/**
 * Derive every surface name from a 6-octet MAC address.
 *
 * The suffix is the last two MAC octets in uppercase hex. Those are the
 * device-specific part of the address, so two units from the same batch differ
 * there. The mDNS name repeats it in lowercase because DNS labels are compared
 * case-insensitively and lowercase avoids surprising the user with mixed case.
 *
 * @param mac  six octets, as returned by esp_read_mac()
 * @param out  filled in on success, untouched otherwise
 * @return HK_IDENTITY_OK, or a negative ::hk_identity_err_t
 */
int hk_identity_from_mac(const uint8_t mac[6], hk_identity_t *out);

/**
 * Check that a name is a valid single DNS label: 1-63 characters of
 * [a-z0-9-], not starting or ending with a hyphen.
 *
 * @return 1 when valid, 0 otherwise.
 */
int hk_identity_is_valid_mdns_label(const char *label);

#endif /* HK_IDENTITY_H */
