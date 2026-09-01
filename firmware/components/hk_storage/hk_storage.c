#include "hk_storage.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "hk_store";

/** Key holding the schema version in each store. */
#define HK_STORAGE_VERSION_KEY "schema"

static hk_schema_action_t s_user_action = HK_SCHEMA_WRITE_DEFAULTS;
static hk_schema_action_t s_factory_action = HK_SCHEMA_FAIL_SAFE;

/** Read a store's schema version, distinguishing absent from unreadable. */
/**
 * Read the stored schema version.
 *
 * Returns the raw number as well as whether one was there, because deciding
 * what to do needs the version itself: "convert this store" is only a usable
 * answer if a converter for THAT version exists, and hk_schema_plan() cannot
 * check that from a classification alone.
 *
 * @param corrupt set when the key exists but could not be read; the caller
 *                must treat that as unreadable rather than as absent.
 */
static bool read_version(nvs_handle_t handle, uint32_t *out_version, bool *corrupt)
{
    uint32_t stored = 0;
    *corrupt = false;
    esp_err_t err = nvs_get_u32(handle, HK_STORAGE_VERSION_KEY, &stored);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "reading %s: %s", HK_STORAGE_VERSION_KEY, esp_err_to_name(err));
        *corrupt = true;
        return false;
    }
    *out_version = stored;
    return true;
}

/** Plan for one store, treating an unreadable version as corrupt. */
static hk_schema_action_t plan_for(nvs_handle_t handle, hk_store_t store,
                                   uint16_t current, hk_schema_found_t *found_out)
{
    uint32_t version = 0;
    bool corrupt = false;
    const bool present = read_version(handle, &version, &corrupt);

    if (corrupt) {
        *found_out = HK_SCHEMA_FOUND_CORRUPT;
        return hk_schema_resolve(store, HK_SCHEMA_FOUND_CORRUPT);
    }
    *found_out = hk_schema_classify(present, version, current);
    /* hk_schema_plan(), not hk_schema_resolve(): resolve can answer MIGRATE,
     * and nothing here knows how to convert anything. Acting on MIGRATE would
     * read an old layout as though it were the current one. */
    return hk_schema_plan(store, present, version);
}

static esp_err_t init_user_store(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Safe to reformat: this partition holds only user settings, which the
         * owner can set again in a minute. */
        ESP_LOGW(TAG, "user store needs reformatting: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        s_user_action = HK_SCHEMA_WRITE_DEFAULTS;
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open(HK_STORAGE_USER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        s_user_action = HK_SCHEMA_WRITE_DEFAULTS;
        return err;
    }

    hk_schema_found_t found;
    s_user_action = plan_for(handle, HK_STORE_USER,
                             (uint16_t)HK_SCHEMA_USER_VERSION, &found);
    ESP_LOGI(TAG, "user store: %s -> %s",
             hk_schema_found_name(found), hk_schema_action_name(s_user_action));

    if (s_user_action == HK_SCHEMA_WRITE_DEFAULTS) {
        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            err = nvs_set_u32(handle, HK_STORAGE_VERSION_KEY, HK_SCHEMA_USER_VERSION);
        }
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
    }
    nvs_close(handle);
    return err;
}

static void init_factory_store(void)
{
    /* Deliberately no erase-and-retry. On a device that has never been
     * calibrated this simply fails, and failing is the correct answer: the
     * profile is produced once, on a bench, against these drivers. Formatting
     * here would turn "never calibrated" into "calibrated with nothing", which
     * is indistinguishable from a working device right up until a tweeter is
     * driven without protection. */
    esp_err_t err = nvs_flash_init_partition(HK_STORAGE_FACTORY_PARTITION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration store unavailable: %s. This device has no measured "
                      "driver protection profile.", esp_err_to_name(err));
        s_factory_action = HK_SCHEMA_FAIL_SAFE;
        return;
    }

    nvs_handle_t handle;
    /* Read only, always. There is no calibration writer in this firmware; one
     * arrives with G2. A store this build cannot write is a store it cannot
     * corrupt. */
    err = nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                  HK_STORAGE_FACTORY_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration namespace unavailable: %s", esp_err_to_name(err));
        s_factory_action = HK_SCHEMA_FAIL_SAFE;
        return;
    }

    hk_schema_found_t found;
    s_factory_action = plan_for(handle, HK_STORE_FACTORY,
                                (uint16_t)HK_SCHEMA_FACTORY_VERSION, &found);
    nvs_close(handle);

    ESP_LOGI(TAG, "calibration store: %s -> %s",
             hk_schema_found_name(found), hk_schema_action_name(s_factory_action));
    if (!hk_schema_audio_permitted(s_factory_action)) {
        ESP_LOGE(TAG, "no trustworthy calibration: audio must stay in its safe state");
    }
}

esp_err_t hk_storage_init(void)
{
    esp_err_t err = init_user_store();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "user store: %s; running on defaults", esp_err_to_name(err));
    }
    init_factory_store();
    return ESP_OK;  /* An unusable store is reported, never fatal. */
}

hk_schema_action_t hk_storage_factory_action(void) { return s_factory_action; }
hk_schema_action_t hk_storage_user_action(void) { return s_user_action; }

bool hk_storage_audio_permitted(void)
{
    return hk_schema_audio_permitted(s_factory_action);
}

esp_err_t hk_storage_user_reset(void)
{
    /* The only erase in this module, and it names the user namespace inside the
     * default partition. Nothing here can reach HK_STORAGE_FACTORY_PARTITION
     * (PRD-008). */
    nvs_handle_t handle;
    esp_err_t err = nvs_open(HK_STORAGE_USER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, HK_STORAGE_VERSION_KEY, HK_SCHEMA_USER_VERSION);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    ESP_LOGW(TAG, "user settings restored to defaults; calibration untouched");
    return err;
}

bool hk_storage_user_read_u32(const char *key, uint32_t *out)
{
    if (key == NULL || out == NULL) {
        return false;
    }
    nvs_handle_t handle;
    if (nvs_open(HK_STORAGE_USER_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    const bool read = nvs_get_u32(handle, key, out) == ESP_OK;
    nvs_close(handle);
    return read;
}

esp_err_t hk_storage_user_set_u32(const char *key, uint32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(HK_STORAGE_USER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u32(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t hk_storage_factory_get_blob(const char *key, void *out, size_t *length)
{
    if (!hk_schema_audio_permitted(s_factory_action) &&
        s_factory_action == HK_SCHEMA_FAIL_SAFE) {
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                            HK_STORAGE_FACTORY_NAMESPACE,
                                            NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_blob(handle, key, out, length);
    nvs_close(handle);
    return err;
}
