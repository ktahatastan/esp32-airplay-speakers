/**
 * @file hk_network.h
 * @brief Wi-Fi, provisioning transport and mDNS.
 *
 * The hardware layer under hk_provision. The policy module decides when the
 * setup radios should be open; this file starts and stops them, joins Wi-Fi,
 * and publishes the device on the local network under the names hk_identity
 * derives.
 *
 * NOT YET VERIFIED ON HARDWARE. This compiles against ESP-IDF v5.5.1, but no
 * board has run it, and no iOS or Android client has been near it.
 *
 * One transport at a time
 * -----------------------
 * ESP-IDF's provisioning manager keeps a single static context and takes one
 * scheme, so BLE and SoftAP cannot both be live in one session: scheme_ble
 * puts Wi-Fi in station mode while scheme_softap needs AP+station. ADR-0005
 * asks for both to be offered together, which this framework cannot do without
 * a custom protocomm layer. The scheme is therefore a parameter here, and the
 * conflict is recorded in the docs rather than silently resolved.
 *
 * Security
 * --------
 * Provisioning runs with Security 2 (SRP6a). The salt and verifier are
 * per-device and read from the factory_cal namespace; the device never stores
 * the password itself. If they are missing, provisioning does NOT start and
 * does NOT fall back to a weaker mode. A shared or absent credential on a
 * device that accepts Wi-Fi passwords is worse than no provisioning at all.
 */
#ifndef HK_NETWORK_H
#define HK_NETWORK_H

#include <stdbool.h>

#include "esp_err.h"

/** Which provisioning transport this session offers. */
typedef enum {
    HK_NET_SCHEME_SOFTAP = 0, /**< App-less: SoftAP plus a captive portal */
    HK_NET_SCHEME_BLE,        /**< Espressif provisioning apps over BLE.
                                   Requires CONFIG_BT_ENABLED; without it
                                   hk_network_start returns ESP_ERR_NOT_SUPPORTED
                                   rather than silently using another transport. */
} hk_net_scheme_t;

/** What the network layer is doing, for the status LED. */
typedef struct {
    bool provisioning;
    bool connecting;
    bool connected;
    bool error;
} hk_net_status_t;

/** Called whenever the status changes. Runs on the system event task. */
typedef void (*hk_net_status_cb_t)(const hk_net_status_t *status, void *context);

/**
 * Bring up netif, the event loop and Wi-Fi, then either join a stored network
 * or open provisioning.
 *
 * @param scheme    transport to offer if provisioning opens
 * @param callback  status changes; may be NULL
 * @param context   passed back to the callback
 */
esp_err_t hk_network_start(hk_net_scheme_t scheme, hk_net_status_cb_t callback, void *context);

/** Open a provisioning window on a device that already has credentials. */
esp_err_t hk_network_open_provisioning(void);

/** Forget stored Wi-Fi credentials and reopen provisioning. */
esp_err_t hk_network_forget_credentials(void);

/** True when Wi-Fi credentials are stored. */
bool hk_network_is_provisioned(void);

#endif /* HK_NETWORK_H */
