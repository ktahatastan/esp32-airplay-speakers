#include "hk_ota_client.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "hk_ota.h"

static const char *TAG = "hk_ota";

/* A release manifest is a few hundred bytes. Anything far larger is not a
 * manifest, and reading it into RAM before parsing would be the wrong way to
 * find that out. */
#define HK_OTA_MANIFEST_MAX 4096
#define HK_OTA_HTTP_TIMEOUT_MS 15000

/** Copy a JSON string field, recording that it was present and fitted. */
static bool take_string(const cJSON *root, const char *key, char *out, size_t out_size,
                        uint32_t bit, uint32_t *present)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    const size_t len = strlen(item->valuestring);
    if (len == 0u || len >= out_size) {
        /* Too long is refused rather than truncated. A truncated product name
         * or sha256 would go on to be compared, and might even match. */
        return false;
    }
    memcpy(out, item->valuestring, len + 1u);
    *present |= bit;
    return true;
}

/** Copy a JSON number field that must be a non-negative integer. */
static bool take_u32(const cJSON *root, const char *key, uint32_t *out,
                     uint32_t bit, uint32_t *present)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    if (item->valuedouble < 0.0 || item->valuedouble > 4294967295.0) {
        return false;
    }
    /* Reject a fractional size or version outright; rounding one would invent
     * a number the publisher did not write. */
    if ((double)(uint32_t)item->valuedouble != item->valuedouble) {
        return false;
    }
    *out = (uint32_t)item->valuedouble;
    *present |= bit;
    return true;
}

static esp_err_t fetch_manifest(const char *url, char *buffer, size_t buffer_size)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = HK_OTA_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    const int64_t length = esp_http_client_fetch_headers(client);
    if (length > (int64_t)(buffer_size - 1u)) {
        ESP_LOGE(TAG, "manifest too large: %lld bytes", (long long)length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }

    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "manifest HTTP %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* A chunked response reports length -1, so read until the body ends rather
     * than trusting the header. The cap is enforced on what actually arrives. */
    size_t total = 0;
    while (total < buffer_size - 1u) {
        const int got = esp_http_client_read(client, buffer + total,
                                             (int)(buffer_size - 1u - total));
        if (got < 0) {
            err = ESP_FAIL;
            break;
        }
        if (got == 0) {
            break;
        }
        total += (size_t)got;
    }
    buffer[total] = '\0';

    if (err == ESP_OK && !esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "manifest truncated at %u bytes", (unsigned)total);
        err = ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static bool parse_manifest(const char *json, hk_manifest_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "manifest is not valid JSON");
        return false;
    }

    memset(out, 0, sizeof(*out));
    uint32_t present = 0;

    (void)take_string(root, "product", out->product, sizeof(out->product),
                      HK_MF_PRODUCT, &present);
    (void)take_string(root, "version", out->version, sizeof(out->version),
                      HK_MF_VERSION, &present);
    (void)take_string(root, "channel", out->channel, sizeof(out->channel),
                      HK_MF_CHANNEL, &present);
    (void)take_string(root, "target", out->target, sizeof(out->target),
                      HK_MF_TARGET, &present);
    (void)take_string(root, "hw_revision", out->hw_revision, sizeof(out->hw_revision),
                      HK_MF_HW_REVISION, &present);
    (void)take_string(root, "asset", out->asset, sizeof(out->asset),
                      HK_MF_ASSET, &present);
    (void)take_string(root, "sha256", out->sha256, sizeof(out->sha256),
                      HK_MF_SHA256, &present);
    (void)take_string(root, "min_updater_version", out->min_updater_version,
                      sizeof(out->min_updater_version), HK_MF_MIN_UPDATER, &present);
    (void)take_u32(root, "size", &out->size, HK_MF_SIZE, &present);
    (void)take_u32(root, "secure_version", &out->secure_version, HK_MF_SECURE_VER, &present);

    out->present = present;
    cJSON_Delete(root);
    return true;
}

esp_err_t hk_ota_client_run(const hk_ota_request_t *request)
{
    if (request == NULL || request->manifest_url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Gates first: they are free, and a blocked device should not even open a
     * socket. */
    const hk_gate_result_t gate =
        hk_gate_evaluate(&request->gate_inputs, request->gate_limits);
    if (gate != HK_GATE_GO) {
        ESP_LOGI(TAG, "not now: %s", hk_gate_result_name(gate));
        return ESP_ERR_INVALID_STATE;
    }

    if (!hk_ota_asset_url_ok(request->manifest_url)) {
        ESP_LOGE(TAG, "manifest URL refused");
        return ESP_ERR_INVALID_ARG;
    }

    char *json = malloc(HK_OTA_MANIFEST_MAX);
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = fetch_manifest(request->manifest_url, json, HK_OTA_MANIFEST_MAX);
    if (err != ESP_OK) {
        free(json);
        return err;
    }

    hk_manifest_t manifest;
    const bool parsed = parse_manifest(json, &manifest);
    free(json);
    if (!parsed) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const hk_manifest_err_t verdict = hk_manifest_validate(&manifest, &request->device);
    if (verdict == HK_MANIFEST_ERR_NOT_NEWER) {
        ESP_LOGI(TAG, "already current at %s", request->device.running_version);
        return ESP_ERR_NOT_FOUND;
    }
    if (verdict != HK_MANIFEST_OK) {
        ESP_LOGW(TAG, "manifest refused: %s", hk_manifest_err_name(verdict));
        return ESP_ERR_INVALID_VERSION;
    }

    if (!hk_ota_asset_url_ok(manifest.asset)) {
        ESP_LOGE(TAG, "asset URL refused");
        return ESP_ERR_INVALID_VERSION;
    }

    ESP_LOGI(TAG, "updating %s -> %s", request->device.running_version, manifest.version);

    esp_http_client_config_t http = {
        .url = manifest.asset,
        .timeout_ms = HK_OTA_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http,
    };

    esp_https_ota_handle_t handle = NULL;
    err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota begin failed: %s", esp_err_to_name(err));
        return err;
    }

    /* What ESP-IDF checked here is the descriptor's magic word, and nothing
     * else. project_name is never compared anywhere in the OTA path, so this
     * is the only place a foreign image gets caught. */
    esp_app_desc_t incoming;
    hk_ota_image_t image = {0};
    if (esp_https_ota_get_img_desc(handle, &incoming) == ESP_OK) {
        image.valid = true;
        /* The descriptor's char arrays are not guaranteed NUL-terminated. */
        memcpy(image.project_name, incoming.project_name, sizeof(incoming.project_name));
        image.project_name[sizeof(incoming.project_name)] = '\0';
        memcpy(image.version, incoming.version, sizeof(incoming.version));
        image.version[sizeof(incoming.version)] = '\0';
        image.secure_version = incoming.secure_version;
    }

    const hk_ota_image_err_t image_verdict = hk_ota_image_check(&image, &manifest);
    if (image_verdict != HK_OTA_IMAGE_OK) {
        ESP_LOGE(TAG, "image refused: %s", hk_ota_image_err_name(image_verdict));
        (void)esp_https_ota_abort(handle);
        return ESP_ERR_INVALID_VERSION;
    }

    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        /* Deliberately quiet. A progress log per chunk would drown every other
         * line in the boot report for no benefit at four devices. */
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "download failed: %s", esp_err_to_name(err));
        (void)esp_https_ota_abort(handle);
        return err;
    }
    if (!esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "download ended short");
        (void)esp_https_ota_abort(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota finish failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "%s written; it boots on the next restart", manifest.version);
    return ESP_OK;
}
