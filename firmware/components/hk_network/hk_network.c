#include "hk_network.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
/* The BLE scheme only exists when Bluetooth is compiled in. Guarding it here
 * rather than always enabling Bluetooth keeps the radio, its flash footprint
 * and its RAM out of a build that uses the SoftAP path. */
#ifdef CONFIG_BT_ENABLED
#include "wifi_provisioning/scheme_ble.h"
#endif

#include "hk_identity.h"
#include "hk_provision.h"

static const char *TAG = "hk_net";

/**
 * Where per-device provisioning credentials live.
 *
 * The factory_cal partition, not the user settings one: a user reset must not
 * be able to destroy the salt and verifier printed on the label (PRD-008).
 */
#define HK_PROV_NVS_NAMESPACE "factory_cal"
#define HK_PROV_NVS_SALT      "prov_salt"
#define HK_PROV_NVS_VERIFIER  "prov_verif"

/* SRP6a salt is 16 bytes; the verifier for the 3072-bit group is 384. */
#define HK_PROV_SALT_LEN 16
#define HK_PROV_VERIFIER_LEN 384

/** Consecutive join attempts before the policy module is told it failed. */
#define HK_NET_RETRY_LIMIT 5

static hk_identity_t      s_identity;
static hk_net_scheme_t    s_scheme;
static hk_net_status_cb_t s_callback;
static void              *s_context;
static hk_net_status_t    s_status;
static int                s_retries;
static uint8_t            s_salt[HK_PROV_SALT_LEN];
static uint8_t            s_verifier[HK_PROV_VERIFIER_LEN];
static bool               s_have_security2;

static void publish_status(void)
{
    if (s_callback != NULL) {
        s_callback(&s_status, s_context);
    }
}

/**
 * Load the per-device SRP6a salt and verifier.
 *
 * These are written once at manufacturing time, together with the QR label.
 * The password itself is never stored on the device, which is the point of
 * Security 2: reading the flash does not yield the credential.
 */
static esp_err_t load_security2_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(HK_PROV_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t salt_len = sizeof(s_salt);
    size_t verifier_len = sizeof(s_verifier);
    err = nvs_get_blob(handle, HK_PROV_NVS_SALT, s_salt, &salt_len);
    if (err == ESP_OK) {
        err = nvs_get_blob(handle, HK_PROV_NVS_VERIFIER, s_verifier, &verifier_len);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }
    if (salt_len != HK_PROV_SALT_LEN || verifier_len != HK_PROV_VERIFIER_LEN) {
        ESP_LOGE(TAG, "provisioning credentials are the wrong size (%u/%u)",
                 (unsigned)salt_len, (unsigned)verifier_len);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
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
        s_status.connecting = true;
        publish_status();
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        s_status.connected = false;
        if (s_retries < HK_NET_RETRY_LIMIT) {
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
        break;

    case WIFI_PROV_CRED_FAIL:
        /* The password was wrong or the network was unreachable. The radios
         * stay up so the user can see the result and try again. */
        ESP_LOGE(TAG, "provisioning failed; setup stays open");
        s_status.error = true;
        publish_status();
        break;

    case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "provisioning succeeded");
        s_status.error = false;
        publish_status();
        break;

    case WIFI_PROV_END:
        /* Everything the manager allocated goes back, including the BLE stack
         * when the BLE scheme was used. ADR-0005 requires that it not be left
         * running during normal operation. */
        wifi_prov_mgr_deinit();
        s_status.provisioning = false;
        publish_status();
        ESP_LOGI(TAG, "provisioning closed and its memory released");
        break;

    default:
        break;
    }
}

/** Start provisioning with Security 2, or refuse. */
static esp_err_t start_provisioning(void)
{
    if (!s_have_security2) {
        /* Deliberately fatal to provisioning rather than a downgrade. Accepting
         * a Wi-Fi password over a channel secured by a shared or absent
         * credential is worse than refusing to accept one at all. */
        ESP_LOGE(TAG, "no per-device provisioning credentials in %s; refusing to open "
                      "provisioning rather than fall back to a weaker security mode",
                 HK_PROV_NVS_NAMESPACE);
        return ESP_ERR_NOT_FOUND;
    }

    wifi_prov_security2_params_t security_params = {
        .salt = (const char *)s_salt,
        .salt_len = sizeof(s_salt),
        .verifier = (const char *)s_verifier,
        .verifier_len = sizeof(s_verifier),
    };

    /* service_key is the SoftAP password. NULL leaves the setup network open,
     * which the captive-portal flow needs; the session itself is protected by
     * Security 2, so the password never crosses in clear text. */
    return wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_2, &security_params,
                                            s_identity.softap, NULL);
}

static esp_err_t init_provisioning_manager(void)
{
    if (s_scheme == HK_NET_SCHEME_BLE) {
#ifdef CONFIG_BT_ENABLED
        /* FREE_BTDM releases the whole Bluetooth stack once provisioning ends,
         * which is what ADR-0005 requires: BLE must not stay up during normal
         * playback. */
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        };
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

esp_err_t hk_network_start(hk_net_scheme_t scheme, hk_net_status_cb_t callback, void *context)
{
    s_scheme = scheme;
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
    if (scheme == HK_NET_SCHEME_SOFTAP) {
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "wifi init");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   on_wifi_event, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   on_ip_event, NULL), TAG, "ip events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                   on_prov_event, NULL), TAG, "prov events");

    esp_err_t credentials = load_security2_credentials();
    s_have_security2 = (credentials == ESP_OK);
    if (!s_have_security2) {
        ESP_LOGE(TAG, "provisioning credentials unavailable: %s. This device cannot be "
                      "provisioned until they are written at manufacturing time.",
                 esp_err_to_name(credentials));
    }

    ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "prov mgr init");

    bool provisioned = false;
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_is_provisioned(&provisioned), TAG, "is provisioned");

    if (provisioned) {
        /* Nothing to set up. Release the manager and just join. */
        wifi_prov_mgr_deinit();
        ESP_LOGI(TAG, "credentials found, joining as %s", s_identity.mdns);
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "sta mode");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
        return ESP_OK;
    }

    esp_err_t err = start_provisioning();
    if (err != ESP_OK) {
        wifi_prov_mgr_deinit();
    }
    return err;
}

esp_err_t hk_network_open_provisioning(void)
{
    ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "prov mgr init");
    esp_err_t err = start_provisioning();
    if (err != ESP_OK) {
        wifi_prov_mgr_deinit();
    }
    return err;
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
