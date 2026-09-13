/**
 * @file hk_network.h
 * @brief Wi-Fi, provisioning transport and mDNS.
 *
 * The hardware layer under hk_provision. The policy module decides when the
 * setup radios should be open; this file starts and stops them, joins Wi-Fi,
 * and publishes the device on the local network under the names hk_identity
 * derives.
 *
 * What has and has not run on hardware: the Security 2 / WPA2 design this file
 * carried until 2026-09-13 was set up end to end over BLE on the devkit
 * (2026-09-05) and brought its window up on the product board (2026-09-08).
 * The design below -- Security 1 without a proof of possession, an open setup
 * network -- has NOT been exercised on any board, and no iOS or Android client
 * has been near it. It compiles against ESP-IDF v5.5.1 and its pure parts are
 * host-tested; everything else is a claim for a bench (ADR-0023).
 *
 * Transports
 * ----------
 * ESP-IDF's provisioning manager keeps a single static context and takes one
 * scheme, so the MANAGER can drive BLE or SoftAP, not both. ADR-0005 answered
 * that by offering one transport at a time; ADR-0016 supersedes it: since
 * ADR-0015 the app-less leg no longer needs the manager at all (hk_portal hands
 * credentials over with wifi_prov_mgr_configure_sta()), so with
 * CONFIG_HK_DUAL_TRANSPORT both legs open together at first boot -- the manager
 * runs BLE, and this file raises the access point and the portal beside it.
 * One honest limit: closing a BLE window releases the Bluetooth controller's
 * memory for the rest of the boot (FREE_BTDM), so a window reopened later this
 * boot offers SoftAP only. hk_network_scheme_for() says the same in code.
 *
 * Without dual transport the ADR-0005 rule applies, which transport opens
 * being decided by how provisioning was entered, never by the caller: nothing
 * stored opens SoftAP, a button on a configured device opens BLE.
 * CONFIG_HK_FIRST_BOOT_BLE flips the first row.
 *
 * The app-less half was a promise with nothing behind it until ADR-0015, and
 * hardware showed it on 2026-09-05: joining the SoftAP opened nothing, because
 * wifi_prov_scheme_softap serves protocomm endpoints at 192.168.4.1 and not a
 * web page. The portal is now ours -- hk_portal serves the page and answers
 * every DNS query so a phone opens it by itself. That a phone actually opens
 * the sheet has still not been measured on any board.
 *
 * Security
 * --------
 * Both legs run protocomm Security 1 with a NULL proof of possession
 * (ADR-0023). The device advertises `no_pop`; the session key is the X25519
 * shared secret alone and the payload is AES-CTR. That hides the home Wi-Fi
 * password from a passive listener on the app path and authenticates neither
 * side: anyone in radio range while a window is open can provision the
 * speaker, and nothing proves to the phone that the peer is this speaker. The
 * setup network is open (WIFI_AUTH_OPEN), so the app-less portal form posts
 * the home password in the clear over an unencrypted link -- see hk_portal.h.
 * Nothing in factory_cal is needed for setup to open; the three legacy keys a
 * pre-ADR-0023 board carries there are never read (hk_storage.h). The owner
 * accepted this for a home, and only for a home. A stronger mode comes back
 * only through an ADR that supersedes ADR-0023.
 */
#ifndef HK_NETWORK_H
#define HK_NETWORK_H

#include <stdbool.h>

#include "esp_err.h"

/**
 * Which provisioning transport a session offers.
 *
 * Chosen by hk_network from the situation, not passed in. Exposed so it can be
 * logged and reasoned about.
 */
typedef enum {
    HK_NET_SCHEME_SOFTAP = 0, /**< App-less: an OPEN SoftAP plus hk_portal's own
                                   captive portal (ADR-0015 for the portal,
                                   ADR-0023 for the open network). */
    HK_NET_SCHEME_BLE,        /**< Espressif provisioning apps over BLE.
                                   Needs CONFIG_BT_ENABLED; without it the call
                                   fails with ESP_ERR_NOT_SUPPORTED rather than
                                   silently using another transport. */
} hk_net_scheme_t;

/**
 * The transport that a given situation opens. See the note above for why.
 *
 * One answer overrides every rule in that note: once a provisioning window that
 * used BLE has closed, FREE_BTDM has handed the Bluetooth controller's memory
 * back and no reboot-free path exists to get it again. From that point this
 * returns HK_NET_SCHEME_SOFTAP whatever the situation, which is what ADR-0016
 * already said the product does -- both transports at first boot, BLE only
 * until that window closes.
 */
hk_net_scheme_t hk_network_scheme_for(bool has_credentials);

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
 * or open provisioning on the transport that fits the situation.
 *
 * @param callback  status changes; may be NULL
 * @param context   passed back to the callback
 */
esp_err_t hk_network_start(hk_net_status_cb_t callback, void *context);

/**
 * Open a provisioning window from a button press.
 *
 * On a configured device this opens BLE, unless an earlier window already
 * released the BLE stack this boot -- see hk_network_scheme_for(). On one with
 * no credentials provisioning is already open and this does nothing, so a stray
 * press cannot tear down a setup session the user is in the middle of.
 *
 * Opening is not free: the station is disconnected first so the provisioning
 * manager can run its opening scan, which on a joined speaker means leaving the
 * house network and stopping playback. hk_provision decides whether a press
 * gets this far, and on a working speaker it requires two (HK_PROV_CONFIRM_MS).
 *
 * If the window fails to open, the station is reconnected before returning: the
 * disconnect has already happened by then, and a failure that left the speaker
 * off the network with no way in would be worse than the press doing nothing.
 */
esp_err_t hk_network_open_provisioning(void);

/** Forget stored Wi-Fi credentials and reopen provisioning. */
/**
 * Shut the setup radios.
 *
 * The counterpart to hk_network_open_provisioning(), and the reason the
 * bounded window in hk_provision means anything: without it the policy could
 * decide the window had expired and nothing would happen. A device left
 * advertising BLE all evening because nobody could close it is the failure
 * this prevents.
 *
 * Safe to call when provisioning is already shut.
 */
esp_err_t hk_network_close_provisioning(void);

esp_err_t hk_network_forget_credentials(void);

/** True when Wi-Fi credentials are stored. */
bool hk_network_is_provisioned(void);

/**
 * Signal strength of the associated access point, in dBm.
 *
 * A diagnostic: the network layer logs it once per join, so a log from a
 * speaker that keeps dropping out says whether the signal was the reason.
 * Returns false when nothing is associated, which is the honest answer: a
 * device that is provisioning has no signal to report rather than a weak one.
 */
bool hk_network_rssi(int *dbm);

#endif /* HK_NETWORK_H */
