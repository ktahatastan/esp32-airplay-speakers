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

/**
 * Start the web server and the captive DNS responder.
 *
 * The server handle is returned because the provisioning manager has to be
 * given it (wifi_prov_scheme_softap_set_httpd_handle) BEFORE provisioning
 * starts: protocomm then publishes its own endpoints on this same server
 * instead of starting a second one on the same port.
 */
esp_err_t hk_portal_start(httpd_handle_t *out_server);

/** Stop the DNS responder and the server. Safe to call when not started. */
void hk_portal_stop(void);

/** Report what happened to credentials the portal handed over. */
void hk_portal_set_state(hk_portal_state_t state);

#endif /* HK_PORTAL_H */
