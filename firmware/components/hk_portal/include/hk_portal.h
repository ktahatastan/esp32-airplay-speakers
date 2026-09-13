/**
 * The app-less setup path: a captive portal on the speaker's own setup network.
 *
 * ADR-0015 made it a plain HTML form and ADR-0023 made the network it sits on
 * OPEN. Read together, that means exactly this: the page is served over
 * cleartext HTTP on an unencrypted 802.11 link, so the home Wi-Fi password the
 * user types travels in the clear during the POST, readable by anyone in radio
 * range with a monitor-mode adapter, and an evil twin advertising the same SSID
 * would collect it just as easily. There is no proof of possession; whoever
 * joins the setup network reaches this page. The page says so in one sentence
 * above the form, because the person typing the password is the one taking
 * the risk. The portal cannot do better on its own: a browser at
 * http://192.168.4.1 has no crypto it can lend to a plain page, and a soft-AP
 * on this chip offers no OWE. The alternatives were a key on the network --
 * which is the PIN the owner asked to remove -- or dropping this path; the
 * owner chose neither, for a home, and ADR-0023 records the acceptance and its
 * limit. The BLE path does not share this exposure: it runs protocomm Security
 * 1 and is encrypted against a listener even without a proof of possession.
 *
 * The portal is not a second way to provision. It hands what the user typed to
 * ESP-IDF's provisioning manager through wifi_prov_mgr_configure_sta(), so the
 * connecting, the retries, the success event and the closing of the window all
 * stay in the one state machine the app path already uses.
 */
#ifndef HK_PORTAL_H
#define HK_PORTAL_H

#include "esp_err.h"
#include "esp_http_server.h"

#include "hk_portal_form.h"

/** What the page should tell the user right now. */
typedef enum {
    HK_PORTAL_IDLE = 0,   /**< Waiting for someone to fill the form. */
    HK_PORTAL_APPLYING,   /**< Credentials handed over; the speaker is trying. */
    HK_PORTAL_FAILED,     /**< The network refused them. The form stays open. */
    HK_PORTAL_SUCCEEDED,  /**< Joined. The window is about to close. */
} hk_portal_state_t;

/** Start the web server and the captive DNS responder. */
esp_err_t hk_portal_start(void);

/**
 * The address of the portal's own server handle.
 *
 * This is what wifi_prov_scheme_softap_set_httpd_handle() wants, and the
 * distinction is not pedantic. That function's parameter is documented as
 * "Handle to HTTPD server instance", but protocomm stores the pointer and then
 * dereferences it -- protocomm_httpd.c calls
 * httpd_register_uri_handler(*server, ...) on it. Passing the handle VALUE
 * makes protocomm read a server struct out of the handle's own numeric value,
 * which is a LoadProhibited panic the moment provisioning starts. Measured on
 * the product board on 2026-09-08, reproducibly, as a boot loop.
 *
 * It returns the address of a static, not of the caller's variable, because the
 * lifetime is the second half of the same trap: protocomm keeps the pointer for
 * as long as the window is open, so a handle held in a stack frame would be a
 * use-after-return that outlives the function that set it up.
 *
 * Teardown is safe: protocomm frees the pointer only when it allocated it
 * itself (ext_handle_provided false), and this path always sets that flag.
 */
httpd_handle_t *hk_portal_server_slot(void);

/** Stop the DNS responder and the server. Safe to call when not started. */
void hk_portal_stop(void);

/** Report what happened to credentials the portal handed over. */
void hk_portal_set_state(hk_portal_state_t state);

#endif /* HK_PORTAL_H */
