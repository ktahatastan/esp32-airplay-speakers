#include "hk_network.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <stdio.h>

#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
/* The BLE scheme only exists when Bluetooth is compiled in. Guarding it here
 * rather than always enabling Bluetooth keeps the radio, its flash footprint
 * and its RAM out of a build that uses the SoftAP path. */
#ifdef CONFIG_BT_ENABLED
#include "wifi_provisioning/scheme_ble.h"
#endif

#include "hk_identity.h"
#include "hk_portal.h"
#include "hk_provision.h"

static const char *TAG = "hk_net";

/*
 * Setup security, in one place (ADR-0023).
 *
 * Both legs run protocomm Security 1 with no proof of possession: the manager
 * is started with WIFI_PROV_SECURITY_1 and NULL security parameters, which
 * ESP-IDF v5.5.1 accepts and answers by advertising `no_pop` in proto-ver
 * (wifi_provisioning/src/manager.c, wifi_prov_mgr_start_provisioning). The
 * session key is then the X25519 shared secret alone -- protocomm folds
 * SHA256(pop) into it only when a pop exists (protocomm/src/security/
 * security1.c) -- so the app path is AES-CTR encrypted against a passive
 * listener and authenticated to nobody. Security 0 would remove the encryption
 * too and, on a BLE link this project does not force-encrypt, put the home
 * Wi-Fi password on the air in the clear; it also costs the user a setting in
 * Espressif's stock apps, which refuse an unsecured device until told
 * otherwise. Security 2 needs a per-device salt and verifier and refuses NULL
 * parameters (protocomm/src/common/protocomm.c, protocomm_set_security, the
 * ver == 2 branch), so there is no PIN-less Security 2 to keep.
 *
 * The setup network is OPEN: no WPA2 key, no per-device secret, nothing read
 * from factory_cal before setup can start. Boards flashed before ADR-0023
 * still carry the three legacy provisioning keys in that partition; this file
 * never reads them (hk_storage.h names them and says why they are harmless).
 * The owner's acceptance is for a home; what it costs is written in ADR-0023
 * and in hk_portal.h, not repeated here.
 */

/** Consecutive join attempts before the policy module is told it failed. */
#define HK_NET_RETRY_LIMIT 5

static hk_identity_t      s_identity;
static hk_net_scheme_t    s_scheme;
static hk_net_status_cb_t s_callback;
static void              *s_context;
static hk_net_status_t    s_status;
static int                s_retries;

/**
 * Setup has the radio, so nothing else may fight it for it.
 *
 * Set when provisioning is about to start and cleared when it ends -- not the
 * same span as s_status.provisioning, which only becomes true once the manager
 * reports WIFI_PROV_START. The gap between the two is exactly where the bug
 * lived: the station goes down during it, the retry branch fires, and the
 * connect it starts is still in flight when the manager asks for a scan.
 */
static bool               s_setup_owns_radio;

/**
 * The transport the manager that is currently initialised was bound to.
 *
 * Not the same thing as s_scheme, which is the transport the NEXT window should
 * use. They differ for as long as one window is being torn down while the
 * decision for the following one has already been taken -- which is exactly the
 * moment the flag below is set, so keying off s_scheme there would record the
 * release of a stack that was never up.
 */
static hk_net_scheme_t    s_mgr_scheme;

/**
 * The Bluetooth controller's memory has been handed back, and is not coming
 * back before a reboot.
 *
 * WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM calls esp_bt_mem_release() when
 * the manager is deinitialised (ESP-IDF v5.5.1, scheme_ble.c, WIFI_PROV_DEINIT).
 * ADR-0016 states the consequence as a product sentence: both transports at
 * first boot, BLE only until that window closes.
 */
static bool               s_ble_released;

/**
 * Release the provisioning manager, and record what that cost.
 *
 * Every deinit goes through here, because one of them is irreversible: with the
 * BLE scheme bound, the manager's teardown releases the Bluetooth controller's
 * memory and this boot has no way to get it back. Nothing else in the file has
 * to remember that; it asks hk_network_scheme_for() instead.
 */
static void prov_mgr_deinit(void)
{
    wifi_prov_mgr_deinit();
    if (s_mgr_scheme == HK_NET_SCHEME_BLE && !s_ble_released) {
        s_ble_released = true;
        ESP_LOGI(TAG, "the ble stack went back to the heap; setup reopened later this "
                      "boot will offer the access point only");
    }
}

static void publish_status(void)
{
    if (s_callback != NULL) {
        s_callback(&s_status, s_context);
    }
}

/** Publish the device on the local network under its documented names. */
static esp_err_t start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        return err;
    }
    err = mdns_hostname_set(s_identity.mdns);
    if (err != ESP_OK) {
        return err;
    }
    return mdns_instance_name_set(s_identity.airplay);
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base != WIFI_EVENT) {
        return;
    }

    switch (id) {
    case WIFI_EVENT_STA_START:
        if (s_setup_owns_radio) {
            /* The SoftAP scheme puts Wi-Fi into APSTA, which starts the station
             * and lands here -- inside the window that deliberately owns the
             * radio. Connecting from here is the same mistake the disconnect
             * branch below already refuses: the manager's first act is a scan,
             * and esp_wifi_scan_start is rejected while a connect is in flight.
             *
             * It also lies to the user. esp_wifi_connect() with nothing stored
             * fails on the spot, and a call that never started produces no
             * STA_DISCONNECTED -- so "connecting" would stay set for as long as
             * the window is open, and the indicator would show "joining Wi-Fi"
             * at a device that is waiting to be told which Wi-Fi. Measured on
             * the product board on 2026-09-08: a steady yellow blink through
             * the whole setup window. */
            ESP_LOGI(TAG, "station started while setup owns the radio; not joining");
            break;
        }
        s_status.connecting = true;
        publish_status();
        {
            const esp_err_t joining = esp_wifi_connect();
            if (joining != ESP_OK) {
                /* Same trap, outside provisioning: no event follows a call that
                 * did not start, so the state has to be left here or never. */
                ESP_LOGW(TAG, "could not start joining: %s", esp_err_to_name(joining));
                s_status.connecting = false;
                publish_status();
            }
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        s_status.connected = false;
        if (s_setup_owns_radio) {
            /* Deliberately not reconnecting.
             *
             * Opening provisioning drops the station, and the reflex is to
             * reconnect. That reflex is what broke setup: the manager's first
             * act is to scan for networks to offer the user, esp_wifi_scan_start
             * is refused while a connect is in progress, and the app shows an
             * empty list. So while the window is open the radio is left free
             * for the thing the window exists to do. The station comes back
             * when provisioning ends. */
            s_status.connecting = false;
            ESP_LOGI(TAG, "station down while setup is open; the radio stays free "
                          "for the scan the setup app asks for");
        } else if (s_retries < HK_NET_RETRY_LIMIT) {
            s_retries++;
            s_status.connecting = true;
            ESP_LOGW(TAG, "disconnected, retry %d of %d", s_retries, HK_NET_RETRY_LIMIT);
            esp_wifi_connect();
        } else {
            /* Out of retries. The policy module decides what happens next;
             * this layer only reports. */
            s_status.connecting = false;
            s_status.error = true;
            ESP_LOGE(TAG, "could not join after %d attempts", HK_NET_RETRY_LIMIT);
        }
        publish_status();
        break;

    default:
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base != IP_EVENT || id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "joined, address " IPSTR, IP2STR(&event->ip_info.ip));

    /* Once per join, not polled: a speaker that drops out in one corner of the
     * room and not another is a placement problem, and this line is what tells
     * the two apart from a log after the fact. */
    int rssi_dbm;
    if (hk_network_rssi(&rssi_dbm)) {
        ESP_LOGI(TAG, "signal %d dBm", rssi_dbm);
    }

    s_retries = 0;
    s_status.connecting = false;
    s_status.connected = true;
    s_status.error = false;
    publish_status();

    if (start_mdns() != ESP_OK) {
        ESP_LOGW(TAG, "mdns did not start; the speaker will not be discoverable by name");
    }
}

static void on_prov_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base != WIFI_PROV_EVENT) {
        return;
    }

    switch (id) {
    case WIFI_PROV_START:
        ESP_LOGI(TAG, "provisioning open over %s",
                 s_scheme == HK_NET_SCHEME_BLE ? "ble" : "softap");
        s_status.provisioning = true;
        publish_status();
        break;

    case WIFI_PROV_CRED_RECV:
        ESP_LOGI(TAG, "credentials received");
        hk_portal_set_state(HK_PORTAL_APPLYING);
        break;

    case WIFI_PROV_CRED_FAIL:
        /* The password was wrong or the network was unreachable. The radios
         * stay up so the user can see the result and try again. */
        ESP_LOGE(TAG, "provisioning failed; setup stays open");
        hk_portal_set_state(HK_PORTAL_FAILED);
        s_status.error = true;
        publish_status();
        break;

    case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "provisioning succeeded");
        hk_portal_set_state(HK_PORTAL_SUCCEEDED);
        s_status.error = false;
        publish_status();
        break;

    case WIFI_PROV_END:
        /* Everything the manager allocated goes back, including the BLE stack
         * when the BLE scheme was used. ADR-0005 requires that it not be left
         * running during normal operation. */
        prov_mgr_deinit();
        /* After the manager, not before: it holds the server this stops, and
         * pulling it out from under protocomm's own teardown is a crash. */
        hk_portal_stop();
#if CONFIG_HK_DUAL_TRANSPORT
        /* The access point was ours, so taking it down is ours too. Nothing else
         * does it: the manager only ever knew about BLE. */
        (void)esp_wifi_set_mode(WIFI_MODE_STA);
#endif
        s_status.provisioning = false;
        publish_status();
        ESP_LOGI(TAG, "provisioning closed and its memory released");

        /* Go back to the network the speaker was on.
         *
         * The flag is cleared above first, on purpose: the disconnect handler
         * reads it, and a connect attempted while it still said "provisioning"
         * would be dropped by the very branch that keeps the radio free.
         *
         * Only when not already connected. A window that closed because the
         * user finished setting the speaker up ends with the manager having
         * joined the new network, and a second connect there would tear down
         * the one thing that just started working. */
        s_setup_owns_radio = false;
        if (!s_status.connected) {
            s_retries = 0;
            const esp_err_t rejoin = esp_wifi_connect();
            if (rejoin != ESP_OK) {
                ESP_LOGW(TAG, "could not rejoin after setup closed: %s",
                         esp_err_to_name(rejoin));
            }
        }
        break;

    default:
        break;
    }
}

#if CONFIG_HK_DUAL_TRANSPORT
/**
 * Bring up the setup access point ourselves.
 *
 * Transcribed from ESP-IDF's own scheme_softap.c, because with dual transport
 * the manager is running the BLE scheme and no scheme is left to do this. The
 * portal does not need one: since ADR-0015 it hands credentials over through
 * wifi_prov_mgr_configure_sta(), so the SoftAP leg needs an interface and a
 * server, not a provisioning scheme.
 */
static esp_err_t start_setup_ap(void)
{
    wifi_config_t ap = {
        .ap = {
            .max_connection = 4,
            /* Open, on purpose (ADR-0023). Exactly the shape scheme_softap.c
             * builds for an empty passphrase: no key, authmode open. The
             * password field stays the zeros the initialiser left in it. What
             * that costs the portal path is stated in hk_portal.h. */
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    /* An SSID is bytes with a length, not a C string -- 32 characters is legal
     * and would leave no room for a terminator. The bound is the SOURCE's size,
     * not the field's: bounding by the 32-byte destination lets the read run off
     * a 22-byte identity, which is what the compiler objected to and it was
     * right. The clamp then keeps a longer name from overrunning the field. */
    size_t ssid_len = strnlen(s_identity.softap, sizeof(s_identity.softap));
    if (ssid_len > sizeof(ap.ap.ssid)) {
        ssid_len = sizeof(ap.ap.ssid);
    }
    memcpy(ap.ap.ssid, s_identity.softap, ssid_len);
    ap.ap.ssid_len = (uint8_t)ssid_len;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "the setup network did not come up: %s", esp_err_to_name(err));
    }
    return err;
}
#endif /* CONFIG_HK_DUAL_TRANSPORT */

/**
 * Open the app-less leg on its own, with the manager driving it.
 *
 * This is the SoftAP scheme's ordinary path, and it is reached from two
 * directions: a build with one transport at a time that has decided on SoftAP,
 * and a dual-transport build reopening setup after the BLE stack has already
 * gone back to the heap (ADR-0016). Neither is a degraded window -- it is the
 * app-less route, whole.
 *
 * The service key is NULL, so the scheme brings the network up open: with an
 * empty passphrase scheme_softap.c sets WIFI_AUTH_OPEN, the same choice
 * start_setup_ap() makes for the dual window. ADR-0015 once keyed this network
 * with a per-device WPA2 password so the portal's plain form had a secret
 * link under it; ADR-0023 gives that up for a setup with nothing to type but
 * the home password, and says what the trade costs. The app that speaks
 * protocomm on this leg still gets Security 1's encryption; the browser on the
 * portal gets none.
 *
 * Every failure path hands the radio back. The caller disconnected the station
 * before calling, so a leg that does not come up must not also leave setup
 * owning a radio it is not using: that flag is what suppresses the reconnect,
 * and stuck true it keeps the speaker off the network until the next reboot.
 */
static esp_err_t start_softap_only(void)
{
    /* Before the manager starts, not after: protocomm publishes the app
     * path's endpoints on whatever server it is given here, and given none
     * it starts a second one on the same port. */
    const esp_err_t portal = hk_portal_start();
    if (portal != ESP_OK) {
        ESP_LOGE(TAG, "the setup page did not come up: %s", esp_err_to_name(portal));
        s_setup_owns_radio = false;
        return portal;
    }
    /* The ADDRESS of the portal's handle, not the handle. protocomm
     * dereferences what it is given and keeps the pointer for the life of
     * the window; see hk_portal_server_slot(). */
    wifi_prov_scheme_softap_set_httpd_handle(hk_portal_server_slot());

    const esp_err_t started =
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL,
                                         s_identity.softap, NULL);
    if (started != ESP_OK) {
        hk_portal_stop();
        s_setup_owns_radio = false;
    }
    return started;
}

/**
 * Open setup on the transport(s) the situation calls for.
 *
 * Nothing is loaded and nothing can be missing: Security 1 with a NULL proof
 * of possession needs no per-device material, and an open network needs no
 * key. A blank factory_cal opens setup like any other board (ADR-0023). The
 * refusal that used to live here -- no salt and verifier, no provisioning --
 * guarded a property this design no longer has.
 */
static esp_err_t start_provisioning(void)
{
    /* Setup takes the radio from here until WIFI_PROV_END.
     *
     * The station is put down deliberately instead of being knocked down by
     * the manager underneath itself, and the reconnect reflex is disarmed
     * first: the manager's opening move is to scan for networks to offer, and
     * esp_wifi_scan_start is refused while a connect is in progress. Left
     * alone, the app shows an empty network list -- the button breaks the very
     * flow it was pressed to start. Harmless when nothing was connected, which
     * is the first-boot case. */
    s_setup_owns_radio = true;
    (void)esp_wifi_disconnect();

    /* The service name is what the user has to find: the setup network's SSID
     * on the SoftAP leg, the advertised name on BLE. Since ADR-0023 the two are
     * one string -- hk_identity spells them the same, with the PROV_ prefix the
     * stock apps filter by -- but each leg is still handed its own field, so
     * the day they are allowed to differ nothing here has to change. Both also
     * appear in the optional QR the apps scan, so a name passed to the wrong
     * leg does not fail loudly; it produces a QR that searches for a device
     * nobody is advertising. */
#if CONFIG_HK_DUAL_TRANSPORT
    if (s_scheme != HK_NET_SCHEME_BLE) {
        /* The BLE stack went back to the heap when an earlier window closed, so
         * there is one leg left and the manager drives it. Asking for the other
         * one here is what turned a second button press into a speaker that had
         * left the network and opened nothing. */
        ESP_LOGI(TAG, "ble is gone for this boot; opening the app-less leg alone");
        return start_softap_only();
    }

    /* What the manager is told to advertise is the BLE name. The access point
     * carries the SSID and is not the manager's business at all. */
    wifi_config_t saved_sta;
    const bool have_saved = (esp_wifi_get_config(WIFI_IF_STA, &saved_sta) == ESP_OK);

    /* NULL where the proof of possession went: ESP-IDF answers by advertising
     * `no_pop`, and the stock apps then open the session with an empty pop and
     * no prompt. See the note at the top of this file. */
    const esp_err_t opened =
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL,
                                         s_identity.ble, NULL);
    if (opened != ESP_OK) {
        s_setup_owns_radio = false;
        return opened;
    }

    /* wifi_prov_mgr_start_provisioning() empties the station config in RAM and
     * puts it back only when it fails. On the success path it stays empty, so
     * wifi_prov_mgr_is_provisioned() -- which reads that live config -- answers
     * "no" for the rest of the boot, and the rejoin at WIFI_PROV_END has nothing
     * to connect to. Putting it back is safe here because the reconnect reflex
     * is disarmed for the length of the window. */
    if (have_saved && saved_sta.sta.ssid[0] != '\0') {
        (void)esp_wifi_set_config(WIFI_IF_STA, &saved_sta);
    }
    memset(&saved_sta, 0, sizeof(saved_sta));

    /* Now the other half. Order matters: the manager has just forced STA, so the
     * access point goes up after it, not before. */
    esp_err_t second = start_setup_ap();
    if (second == ESP_OK) {
        second = hk_portal_start();
    }
    if (second != ESP_OK) {
        /* One leg is not both. Rather than leave a window that some phones can
         * see and others cannot, close it and say so. */
        ESP_LOGE(TAG, "the app-less leg did not come up (%s); closing the window",
                 esp_err_to_name(second));
        hk_portal_stop();
        wifi_prov_mgr_stop_provisioning();
        return second;
    }

    /* The number that decides whether this is affordable, taken where it means
     * something: after NimBLE is advertising, the access point is up and the
     * HTTP server is listening. The boot report's figure is measured before any
     * of that and cannot answer the question. */
    ESP_LOGI(TAG, "provisioning open on both: ble \"%s\" and softap \"%s\"; "
                  "free %u B internal (largest block %u B)",
             s_identity.ble, s_identity.softap,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    return ESP_OK;
#else
    if (s_scheme != HK_NET_SCHEME_BLE) {
        return start_softap_only();
    }

    /* BLE carries no service key (the scheme ignores it) and, since ADR-0023,
     * no proof of possession either: NULL parameters make the manager
     * advertise `no_pop`. */
    const esp_err_t started =
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL,
                                         s_identity.ble, NULL);
    if (started != ESP_OK) {
        /* The station was put down for a window that did not open. Leaving the
         * flag set would suppress the reconnect for the rest of the boot. */
        s_setup_owns_radio = false;
    }
    return started;
#endif /* CONFIG_HK_DUAL_TRANSPORT */
}

hk_net_scheme_t hk_network_scheme_for(bool has_credentials)
{
    /* Before any preference: one of the two transports may not exist any more.
     *
     * ADR-0016 says it plainly -- "Pencere kapandıktan sonra yeniden açılan bir
     * kurulum yalnız SoftAP sunar": a setup reopened after that window has
     * closed offers only SoftAP. The reason is FREE_BTDM, which hands the
     * Bluetooth controller's memory back at manager teardown and cannot be
     * undone before a reboot.
     *
     * The code did not say it. Every branch below hands back BLE for a device
     * that already holds credentials, so the second window a user opens asked
     * the manager for a transport that was gone. That failure arrives AFTER
     * start_provisioning() has put the station down for the manager's scan, and
     * the caller treats it as fatal -- so the press took the speaker off the
     * network and opened nothing at all. The ADR was right and this function was
     * wrong; this is the line that makes them agree.
     *
     * has_credentials is not consulted here on purpose. It is not a preference
     * being overridden, it is the only transport there is. */
    if (s_ble_released) {
        return HK_NET_SCHEME_SOFTAP;
    }

    /* ADR-0005 option C. The app-less path must always be reachable, so a
     * device with nothing stored opens SoftAP; a device that already works
     * opens BLE, because a SoftAP would push the user's phone off the network
     * they are on. Clearing credentials with a 5 s hold returns them to the
     * SoftAP case, which is how the app-less route stays available. */
#if CONFIG_HK_DUAL_TRANSPORT
    /* There is nothing left to derive. Both transports open together and the
     * phone picks, so this only answers which one the MANAGER drives -- and that
     * is always BLE, because the other leg no longer needs it. */
    (void)has_credentials;
    return HK_NET_SCHEME_BLE;
#elif CONFIG_HK_FIRST_BOOT_BLE
    /* Interim, and it does change that rule: the owner sets this speaker up
     * over BLE with a QR, so BLE is the transport a new device should offer. */
    (void)has_credentials;
    return HK_NET_SCHEME_BLE;
#else
    return has_credentials ? HK_NET_SCHEME_BLE : HK_NET_SCHEME_SOFTAP;
#endif
}

static esp_err_t init_provisioning_manager(void)
{
    /* What is about to be bound, so prov_mgr_deinit() knows whether the teardown
     * takes the BLE stack with it. Recorded before the call rather than after:
     * wifi_prov_mgr_init() invokes the scheme's WIFI_PROV_INIT handler on the
     * way in, and a failure part-way through still leaves a bound scheme. */
    s_mgr_scheme = s_scheme;

    if (s_scheme == HK_NET_SCHEME_BLE) {
#ifdef CONFIG_BT_ENABLED
        /* FREE_BTDM releases the whole Bluetooth stack once provisioning ends,
         * which is what ADR-0005 requires: BLE must not stay up during normal
         * playback. */
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        };
#if CONFIG_HK_DUAL_TRANSPORT
        /* The BLE scheme declares WIFI_MODE_STA, and the manager re-applies that
         * declaration when credentials arrive -- which would drop the access
         * point mid-setup, from a code path we do not own. The scheme travels by
         * value into the manager's config, so changing the copy is enough: it
         * tells the manager to leave us in APSTA rather than fighting it back
         * afterwards, which is a race we would lose sometimes. */
        config.scheme.wifi_mode = WIFI_MODE_APSTA;
#endif
        return wifi_prov_mgr_init(config);
#else
        ESP_LOGE(TAG, "the BLE provisioning scheme needs CONFIG_BT_ENABLED, which this "
                      "build does not set");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
    };
    return wifi_prov_mgr_init(config);
}

#if CONFIG_HK_BOARD_DEVKIT_N8R2
/**
 * Put the bench board on a network over USB, because the two normal ways in are
 * both closed on it.
 *
 * The devkit has no button, so the press that clears credentials cannot be
 * given; and joining its SoftAP from the machine that is driving it takes that
 * machine off the network it is being told to join. Neither is a firmware
 * problem and neither is worth a firmware workaround on the product, so this
 * exists only where those two facts hold.
 *
 * It never overwrites credentials that are already stored. A preload that
 * silently replaced a provisioned network would make every bench result
 * ambiguous: nobody could tell which network a board was actually on.
 *
 * The values come from a build-time config that is deliberately empty in the
 * committed tree; see main/Kconfig.projbuild. The password does end up inside
 * the image, which is the honest cost of doing this at all, and the reason it
 * is confined to a board with nothing attached to it.
 */
static void preload_bench_credentials(void)
{
    if (CONFIG_HK_DEVKIT_WIFI_SSID[0] == '\0') {
        return;
    }

    wifi_config_t existing = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &existing) == ESP_OK
        && existing.sta.ssid[0] != '\0') {
        ESP_LOGI(TAG, "bench preload skipped: this board already has a network");
        return;
    }

    wifi_config_t bench = {0};
    (void)snprintf((char *)bench.sta.ssid, sizeof(bench.sta.ssid),
                   "%s", CONFIG_HK_DEVKIT_WIFI_SSID);
    (void)snprintf((char *)bench.sta.password, sizeof(bench.sta.password),
                   "%s", CONFIG_HK_DEVKIT_WIFI_PASSWORD);

    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &bench);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bench preload failed: %s", esp_err_to_name(err));
        return;
    }
    /* The SSID is not a secret and naming it is the point: it says which
     * network this board was put on, which is the first thing to check when a
     * bench result disagrees with expectations. */
    ESP_LOGW(TAG, "bench preload wrote a network for '%s'; this build carries it",
             (const char *)bench.sta.ssid);
}
#endif

esp_err_t hk_network_start(hk_net_status_cb_t callback, void *context)
{
    s_callback = callback;
    s_context = context;
    memset(&s_status, 0, sizeof(s_status));
    s_retries = 0;

    uint8_t mac[6] = {0};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_STA), TAG, "read mac");
    if (hk_identity_from_mac(mac, &s_identity) != HK_IDENTITY_OK) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    esp_netif_create_default_wifi_sta();
    /* The AP interface is created unconditionally: whether SoftAP is needed
     * depends on stored credentials, which cannot be read until the manager is
     * up, and creating a netif is cheap next to discovering too late that it is
     * missing. */
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "wifi init");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   on_wifi_event, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   on_ip_event, NULL), TAG, "ip events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                   on_prov_event, NULL), TAG, "prov events");

#if CONFIG_HK_BOARD_DEVKIT_N8R2
    preload_bench_credentials();
#endif

    /* Said once at boot so a log reads the same way the decision does. Until
     * ADR-0023 this is where the per-device salt, verifier and setup key were
     * loaded from factory_cal and their absence refused setup; there is
     * nothing to load now, and a board with a blank calibration store opens
     * setup like any other. */
    ESP_LOGI(TAG, "setup security: protocomm security 1, no proof of possession; "
                  "setup network open (ADR-0023)");

    /* The manager is needed just to answer "are we provisioned?", and the
     * answer decides the scheme. It is initialised with SoftAP for that query
     * and genuinely reinitialised if the answer names the other.
     *
     * The reinitialisation is not a formality. wifi_prov_mgr_init() binds the
     * transport, and starting provisioning afterwards runs whatever it was
     * bound to -- not what s_scheme says. Skipping it produces a device that
     * logs "provisioning open over ble" while advertising a SoftAP, which is
     * the worst kind of wrong: every surface agrees except the radio. This
     * comment used to promise the reinitialisation while the code did not do
     * it, and the bug stayed hidden only because the unprovisioned case
     * happened to want the scheme the query had already installed. */
    const hk_net_scheme_t query_scheme = HK_NET_SCHEME_SOFTAP;
    s_scheme = query_scheme;
    ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "prov mgr init");

    bool provisioned = false;
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_is_provisioned(&provisioned), TAG, "is provisioned");
    s_scheme = hk_network_scheme_for(provisioned);

    if (!provisioned && s_scheme != query_scheme) {
        prov_mgr_deinit();
        ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "prov mgr reinit");
    }

    if (provisioned) {
        /* Nothing to set up. Release the manager and just join. */
        prov_mgr_deinit();
        ESP_LOGI(TAG, "credentials found, joining as %s", s_identity.mdns);
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "sta mode");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
        return ESP_OK;
    }

    esp_err_t err = start_provisioning();
    if (err != ESP_OK) {
        prov_mgr_deinit();
    }
    return err;
}

esp_err_t hk_network_open_provisioning(void)
{
    if (s_status.provisioning) {
        /* Already open. A stray press must not tear down a setup session the
         * user is in the middle of. */
        ESP_LOGI(TAG, "provisioning is already open; leaving it alone");
        return ESP_OK;
    }

    s_scheme = hk_network_scheme_for(hk_network_is_provisioned());
    ESP_LOGI(TAG, "opening provisioning over %s",
             s_scheme == HK_NET_SCHEME_BLE ? "ble" : "softap");

    ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "prov mgr init");
    esp_err_t err = start_provisioning();
    if (err != ESP_OK) {
        prov_mgr_deinit();

        /* The press already cost the station its connection: start_provisioning()
         * puts it down before it opens anything, so the manager can scan. The
         * window it was spent on did not open, and the STA_DISCONNECTED that
         * fired in between was swallowed by the branch that keeps the radio free
         * for setup -- so nothing else is going to bring the station back.
         *
         * Without this the failure is worse than doing nothing: the speaker
         * leaves the network, opens no way in, and stays that way until it is
         * power-cycled. */
        s_setup_owns_radio = false;
        s_retries = 0;
        const esp_err_t rejoin = esp_wifi_connect();
        if (rejoin == ESP_OK) {
            ESP_LOGW(TAG, "setup did not open (%s); rejoining the network the press left",
                     esp_err_to_name(err));
        } else {
            ESP_LOGW(TAG, "setup did not open (%s) and the rejoin did not start "
                          "either (%s)", esp_err_to_name(err), esp_err_to_name(rejoin));
        }
    }
    return err;
}

esp_err_t hk_network_close_provisioning(void)
{
    if (!s_status.provisioning) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "closing provisioning");
    /* stop_provisioning() before deinit(): ESP-IDF's own documentation warns
     * that deinit alone leaves the transport running. */
    wifi_prov_mgr_stop_provisioning();
    prov_mgr_deinit();
    s_status.provisioning = false;
    publish_status();
    return ESP_OK;
}

esp_err_t hk_network_forget_credentials(void)
{
    ESP_LOGW(TAG, "forgetting stored Wi-Fi credentials");
    /* Clears only the Wi-Fi credentials the manager stored. Factory
     * calibration lives in its own partition and is untouched (PRD-008). */
    ESP_RETURN_ON_ERROR(esp_wifi_restore(), TAG, "wifi restore");
    return hk_network_open_provisioning();
}

bool hk_network_is_provisioned(void)
{
    bool provisioned = false;
    return wifi_prov_mgr_is_provisioned(&provisioned) == ESP_OK && provisioned;
}

bool hk_network_rssi(int *dbm)
{
    wifi_ap_record_t ap;
    if (dbm == NULL || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return false;
    }
    *dbm = ap.rssi;
    return true;
}
