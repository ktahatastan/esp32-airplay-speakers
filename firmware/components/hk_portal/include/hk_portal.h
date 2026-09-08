/**
 * The app-less setup path: a captive portal on the speaker's own setup network.
 *
 * ADR-0015. The setup network is WPA2 and its key is the per-device setup
 * password, so whoever reaches this server has already proved they hold the
 * label. That is what lets the page below be a plain form: the secrecy comes
 * from the link, not from cryptography written in JavaScript and served over
 * cleartext HTTP.
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
