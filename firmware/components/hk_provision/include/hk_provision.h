/**
 * @file hk_provision.h
 * @brief When the setup radios are open, and when they must be shut.
 *
 * Implements the provisioning state machine in
 * docs/controls-and-provisioning-plan.md and ADR-0005. Pure C: the policy is
 * decided here and tested on the host, while the ESP-IDF layer only turns
 * radios on and off when told to.
 *
 * The rules worth stating, because each one is a way to get this wrong:
 *
 *   - On first boot, with no stored credentials, provisioning stays open
 *     indefinitely. A timeout there would strand a device the user has not
 *     finished setting up.
 *   - A window opened deliberately on an already-configured device DOES time
 *     out. Leaving BLE and an open access point advertising forever on a
 *     working speaker is an attack surface, not a convenience.
 *   - The radios stay open across the connection attempt that follows
 *     provisioning. Closing them the instant credentials arrive would cut the
 *     phone off before it learns whether the password was right.
 *   - Once connected, both radios close and the BLE stack is released. ADR-0005
 *     is explicit that BLE is not left running during normal playback.
 */
#ifndef HK_PROVISION_H
#define HK_PROVISION_H

#include <stdbool.h>
#include <stdint.h>

/** A deliberately opened provisioning window lasts this long. */
#define HK_PROV_WINDOW_MS 600000u  /* 10 minutes */

/** Consecutive failed joins before falling back to provisioning. */
#define HK_PROV_MAX_FAILURES 3u

typedef enum {
    HK_PROV_PROVISIONING = 0, /**< BLE and SoftAP open, waiting for the user */
    HK_PROV_CONNECTING,       /**< Trying stored credentials */
    HK_PROV_ONLINE,           /**< Joined; setup radios shut */
} hk_prov_state_t;

typedef enum {
    HK_PROV_EV_TICK = 0,          /**< Time passed; check the window */
    HK_PROV_EV_BUTTON_SHORT,      /**< Short press: open a setup window */
    HK_PROV_EV_CREDENTIALS,       /**< Credentials received over BLE or the portal */
    HK_PROV_EV_CONNECT_OK,
    HK_PROV_EV_CONNECT_FAIL,
    HK_PROV_EV_NETWORK_RESET,     /**< 5 s hold: forget Wi-Fi only */
    HK_PROV_EV_FACTORY_RESET,     /**< 12 s hold: user settings, credentials included */
} hk_prov_event_t;

/** Which setup radios should be running right now. */
typedef struct {
    bool ble;
    bool softap;
} hk_prov_radios_t;

typedef struct {
    hk_prov_state_t state;
    bool     has_credentials;
    bool     radios_open;
    bool     bounded;              /**< Window auto-closes (opened on a configured device) */
    uint32_t opened_ms;
    uint8_t  consecutive_failures;
} hk_prov_t;

/**
 * Initialise from what storage says.
 *
 * @param prov            state to initialise
 * @param has_credentials whether Wi-Fi credentials are already stored
 * @param recovery        button held at boot: force provisioning open regardless
 * @param now_ms          current millisecond tick
 */
void hk_prov_init(hk_prov_t *prov, bool has_credentials, bool recovery, uint32_t now_ms);

/** Apply one event. Safe to call with HK_PROV_EV_TICK at any rate. */
void hk_prov_handle(hk_prov_t *prov, hk_prov_event_t event, uint32_t now_ms);

/** Radios the caller should have running. */
hk_prov_radios_t hk_prov_radios(const hk_prov_t *prov);

/** True once provisioning has closed and the BLE stack may be freed. */
bool hk_prov_ble_releasable(const hk_prov_t *prov);

/** Short name, for logs and tests. */
const char *hk_prov_state_name(hk_prov_state_t state);

#endif /* HK_PROVISION_H */
