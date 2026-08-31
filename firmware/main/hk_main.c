/**
 * @file hk_main.c
 * @brief Harman Kardom application entry point.
 *
 * Stage F0 of docs/03-firmware/firmware-plan.md. This build does exactly one
 * thing: come up, report what it is, and prove the skeleton links. It plays no
 * audio, joins no network and drives no GPIO, because every one of those needs
 * a hardware gate that has not been measured yet:
 *
 *   F1  AirPlay stack is not chosen (ADR-0007 is still open)
 *   F2  audio path needs G1, the amplifier on a dummy load
 *   F3  DSP protection needs G0, the driver impedance measurement
 *   F4  Wi-Fi, mDNS and SoftAP provisioning are live; per-device credentials
 *       must be written before provisioning can open
 *   F5  button and LED are live; storage-backed reset actions wait for F6
 *   F6  user and calibration stores exist; power telemetry needs G4
 *   F7  OTA, needs G6
 *
 * Nothing below may grow into driving a real driver without the matching gate.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_button.h"
#include "hk_identity.h"
#include "hk_led.h"
#include "hk_network.h"
#include "hk_pins.h"
#include "hk_provision.h"
#include "hk_storage.h"
#include "hk_ui.h"
#include "hk_version.h"

static const char *TAG = "hk";

/** Read the factory MAC and derive every surface name from it. */
static esp_err_t report_identity(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
        return err;
    }

    hk_identity_t identity;
    int status = hk_identity_from_mac(mac, &identity);
    if (status != HK_IDENTITY_OK) {
        ESP_LOGE(TAG, "identity derivation failed: %d", status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "device id   %s", identity.suffix);
    ESP_LOGI(TAG, "airplay     %s", identity.airplay);
    ESP_LOGI(TAG, "ble         %s", identity.ble);
    ESP_LOGI(TAG, "softap      %s", identity.softap);
    ESP_LOGI(TAG, "mdns        %s.local", identity.mdns);
    return ESP_OK;
}

/** Log the GPIO assignment so a bring-up session can compare it to the sheet. */
static void report_pins(void)
{
    ESP_LOGI(TAG, "gpio assignment (candidate, see circuit-and-wiring-plan section 3.1)");
    for (int i = 0; i < hk_pin_table_size(); i++) {
        ESP_LOGI(TAG, "  %-10s GPIO%d", hk_pin_table[i].role, hk_pin_table[i].gpio);
    }
}

/**
 * Report the running slot and firmware version.
 *
 * The image is NOT marked valid here. That happens only after the first-boot
 * health check defined in docs/03-firmware/ota-and-release-plan.md, which is
 * F7 work; marking it valid now would defeat the rollback that ADR-0008 relies
 * on. CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE stays off until then, so this build
 * boots normally and the report is informational.
 */
static void report_build(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(TAG, "firmware    %s", app->version);
    ESP_LOGI(TAG, "idf         %s", app->idf_ver);
    ESP_LOGI(TAG, "built       %s %s", app->date, app->time);
    if (running != NULL) {
        ESP_LOGI(TAG, "slot        %s at 0x%08" PRIx32 ", %" PRIu32 " bytes",
                 running->label, running->address, running->size);
    }

    hk_version_t parsed;
    if (hk_version_parse(app->version, &parsed) != HK_VERSION_OK) {
        /* PROJECT_VER comes from firmware/version.txt. If it is not strict
         * SemVer the OTA client can never compare it, so the release pipeline
         * would publish an image no device would accept. */
        ESP_LOGE(TAG, "PROJECT_VER '%s' is not strict SemVer; OTA comparison would fail",
                 app->version);
    }
}

/**
 * Report the hardware actually present.
 *
 * Flash size is read with esp_flash_get_physical_size() rather than
 * esp_flash_get_size(): the latter returns the size recorded in the binary
 * image header, which is whatever CONFIG_ESPTOOLPY_FLASHSIZE was set to. It
 * would echo the build configuration back and never disagree with it.
 *
 * There is deliberately no "flash is too small" branch here. ESP-IDF already
 * refuses to start on a part smaller than the image header claims:
 * esp_flash_init_default_chip() returns ESP_ERR_FLASH_SIZE_NOT_MATCH and
 * startup asserts on it, so such a board panics before app_main runs. A check
 * here could never fire, and a check that cannot fire is worse than none:
 * it reads like assurance.
 *
 * PSRAM is reported because it is the half of "N16R8" that flash size alone
 * cannot distinguish; an N16 board without PSRAM boots fine and would otherwise
 * look correct in this report.
 */
static void report_hardware(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "chip        %d core(s), revision %d", chip.cores, chip.revision);

    uint32_t flash_size = 0;
    esp_err_t err = esp_flash_get_physical_size(NULL, &flash_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "flash       %" PRIu32 " MB detected", flash_size / (1024U * 1024U));
    } else {
        ESP_LOGW(TAG, "flash       size not detected: %s", esp_err_to_name(err));
    }

    size_t psram = esp_psram_get_size();
    if (psram == 0) {
        ESP_LOGE(TAG, "psram       none found; ADR-0010 specifies N16R8 with 8 MB PSRAM");
    } else {
        ESP_LOGI(TAG, "psram       %u MB", (unsigned)(psram / (1024U * 1024U)));
    }
}

/** Provisioning policy for this boot. Nothing drives the radios yet. */
static hk_prov_t s_provisioning;

/**
 * Act on a committed button gesture.
 *
 * Runs in the UI task, so it stays short. The destructive branches log their
 * intent rather than performing it: the storage layer that would carry them out
 * is F6 work, and wiring a half-built erase path to a physical button is how a
 * user loses their calibration by accident.
 */
/**
 * Milliseconds since boot.
 *
 * The provisioning policy is written against a real clock, and until now every
 * call site passed a literal 0. That made the bounded window — the ten minutes
 * after which a setup session opened on an already-configured speaker closes
 * itself — unreachable: no time ever passed, so it could never expire.
 */
static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void on_button(hk_button_event_t event, void *context)
{
    (void)context;
    switch (event) {
    case HK_BUTTON_EVENT_SHORT_PRESS:
        hk_prov_handle(&s_provisioning, HK_PROV_EV_BUTTON_SHORT, now_ms());
        ESP_LOGI(TAG, "button: opening provisioning -> %s",
                 hk_prov_state_name(s_provisioning.state));
        if (hk_network_open_provisioning() != ESP_OK) {
            ESP_LOGE(TAG, "could not open provisioning");
        }
        break;
    case HK_BUTTON_EVENT_NETWORK_RESET:
        ESP_LOGW(TAG, "button: forgetting Wi-Fi credentials");
        hk_prov_handle(&s_provisioning, HK_PROV_EV_NETWORK_RESET, now_ms());
        if (hk_network_forget_credentials() != ESP_OK) {
            ESP_LOGE(TAG, "could not clear credentials");
        }
        break;
    case HK_BUTTON_EVENT_FACTORY_RESET:
        ESP_LOGW(TAG, "button: restoring user settings to defaults");
        hk_prov_handle(&s_provisioning, HK_PROV_EV_FACTORY_RESET, now_ms());
        /* User settings first, then credentials. Calibration is in another
         * partition that this firmware opens read-only, so neither call can
         * reach it (PRD-008). */
        if (hk_storage_user_reset() != ESP_OK) {
            ESP_LOGE(TAG, "could not restore user settings");
        }
        if (hk_network_forget_credentials() != ESP_OK) {
            ESP_LOGE(TAG, "could not clear credentials");
        }
        break;
    case HK_BUTTON_EVENT_NONE:
    default:
        break;
    }
}

/** Mirror the network layer's state onto the status LED. */
static void on_network_status(const hk_net_status_t *network, void *context)
{
    (void)context;
    hk_ui_set_network(network->provisioning, network->connecting, network->connected);
    hk_ui_set_fault(HK_UI_FAULT_NETWORK, network->error);
}

/**
 * Bring up the network.
 *
 * A failure here is reported and survived rather than fatal. The most likely
 * one on a fresh board is that the per-device provisioning credentials have
 * never been written, and a device that reboots forever cannot tell anyone
 * that. It stays up, lights the error state, and says why.
 */
static void start_network(void)
{
    hk_prov_init(&s_provisioning, hk_network_is_provisioned(),
                 hk_ui_recovery_requested(), now_ms());
    ESP_LOGI(TAG, "provisioning policy: %s, bounded=%d",
             hk_prov_state_name(s_provisioning.state), s_provisioning.bounded);

    /* The transport follows the situation rather than a choice made here:
     * SoftAP with nothing stored, BLE from a button press on a configured
     * device. ADR-0005 option C; the reasoning is in hk_network.h. */
    esp_err_t err = hk_network_start(on_network_status, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "network did not start: %s", esp_err_to_name(err));
        hk_ui_set_fault(HK_UI_FAULT_NETWORK, true);
    }

    ESP_LOGI(TAG, "button       short %u-%u ms, network reset %u ms, factory reset %u ms",
             (unsigned)HK_BUTTON_SHORT_MIN_MS, (unsigned)HK_BUTTON_SHORT_MAX_MS,
             (unsigned)HK_BUTTON_NETWORK_MS, (unsigned)HK_BUTTON_FACTORY_MS);
}

void app_main(void)
{
    /* Opens both stores and works out what state each is in. Never fatal: an
     * unusable store is reported, because a speaker that will not boot cannot
     * tell anyone why. */
    ESP_ERROR_CHECK(hk_storage_init());

    ESP_LOGI(TAG, "%s", HK_PRODUCT_FAMILY);
    report_build();
    report_hardware();
    ESP_ERROR_CHECK(report_identity());
    report_pins();
    ESP_LOGI(TAG, "storage     user=%s calibration=%s",
             hk_schema_action_name(hk_storage_user_action()),
             hk_schema_action_name(hk_storage_factory_action()));
    if (!hk_storage_audio_permitted()) {
        ESP_LOGE(TAG, "audio is NOT permitted: this device has no trustworthy driver "
                      "protection profile. No default profile is invented (G0/G2).");
    }

    /* The UI is the one subsystem whose hardware layer exists, so it really
     * runs: the button is read and the LED is driven. */
    ESP_ERROR_CHECK(hk_ui_start(on_button, NULL));
    start_network();
    hk_ui_clear_booting();
    ESP_LOGW(TAG, "no audio in this build. The button, LED and provisioning policy are "
                  "live. See docs/03-firmware/firmware-plan.md for what comes next.");

    /* The supervisory loop. It exists to give the provisioning policy a clock:
     * a window that closes after ten minutes needs something to notice that
     * ten minutes have passed, and an event-driven system has no event for
     * "nothing happened". One second is far finer than the window needs and
     * costs nothing measurable next to the radios. */
    bool radios_were_open = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        hk_prov_handle(&s_provisioning, HK_PROV_EV_TICK, now_ms());

        const hk_prov_radios_t want = hk_prov_radios(&s_provisioning);
        const bool radios_open = want.ble || want.softap;
        if (radios_were_open && !radios_open) {
            ESP_LOGI(TAG, "provisioning window closed after %s",
                     s_provisioning.bounded ? "its bounded timeout" : "success");
            if (hk_network_close_provisioning() != ESP_OK) {
                ESP_LOGE(TAG, "could not close provisioning");
            }
        }
        radios_were_open = radios_open;
    }
}
