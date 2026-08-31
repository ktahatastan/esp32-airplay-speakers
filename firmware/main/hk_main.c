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
 *   F4  provisioning
 *   F5  button and LED
 *   F6  storage and power telemetry, needs G4
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "hk_identity.h"
#include "hk_pins.h"
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

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Erasing here only affects the user settings partition. Factory
         * calibration lives in its own partition and is never touched (PRD-008). */
        ESP_LOGW(TAG, "nvs needs erasing: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "%s -- stage F0 skeleton", HK_PRODUCT_FAMILY);
    report_build();
    report_hardware();
    ESP_ERROR_CHECK(report_identity());
    report_pins();
    ESP_LOGW(TAG, "F0 build: no audio, no network, no GPIO driven. "
                  "See docs/03-firmware/firmware-plan.md for what comes next.");

    /* Idle. A busy loop here would only burn power and hide the log. */
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
