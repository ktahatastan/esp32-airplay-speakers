/*
 * The half of the first-boot check that needs a chip.
 *
 * hk_health.c decides; this collects what the subsystems say, asks it, and
 * carries out the answer. The decision stayed pure so it could be exercised
 * exhaustively on a host; this file is the part that cannot be.
 *
 * The rule that shapes it: DO NOTHING UNLESS THE IMAGE IS PENDING VERIFY. A
 * device flashed over USB, or one running an image confirmed long ago, has
 * nothing to confirm — and calling the rollback path on one of those would
 * reboot a perfectly good speaker into its previous firmware for no reason.
 * Only an image that arrived by OTA and has not yet been judged is in scope.
 */
#include "hk_health.h"

#include <inttypes.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hk_health";

static hk_health_inputs_t s_inputs = {
    .storage = HK_HEALTH_UNKNOWN,
    .network = HK_HEALTH_UNKNOWN,
    /* No hk_audio driver and no ADC driver exist, so these two subsystems
     * cannot report. Declaring that explicitly is the difference between "not
     * applicable to this build" and "has not answered yet": the first lets the
     * image be confirmed, the second eventually rolls it back. They become
     * real criteria in the same change that gives them drivers. */
    .audio = HK_HEALTH_SKIP,
    .telemetry = HK_HEALTH_SKIP,
    .uptime_ms = 0,
    .critical_fault = false,
};
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_settled;      /**< A verdict has been carried out; stop asking. */
static bool s_pending;      /**< This image is awaiting judgement. */
static void (*s_persist)(bool confirmed);

void hk_health_monitor_set_persist(void (*persist)(bool confirmed))
{
    s_persist = persist;
}

void hk_health_report(hk_health_criterion_t which, hk_health_state_t state)
{
    portENTER_CRITICAL(&s_lock);
    switch (which) {
    case HK_HEALTH_CRITERION_STORAGE:   s_inputs.storage = state; break;
    case HK_HEALTH_CRITERION_NETWORK:   s_inputs.network = state; break;
    case HK_HEALTH_CRITERION_AUDIO:     s_inputs.audio = state; break;
    case HK_HEALTH_CRITERION_TELEMETRY: s_inputs.telemetry = state; break;
    }
    portEXIT_CRITICAL(&s_lock);
}

void hk_health_report_fault(void)
{
    portENTER_CRITICAL(&s_lock);
    s_inputs.critical_fault = true;
    portEXIT_CRITICAL(&s_lock);
}

bool hk_health_monitor_begin(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

    s_pending = running != NULL &&
                esp_ota_get_state_partition(running, &state) == ESP_OK &&
                state == ESP_OTA_IMG_PENDING_VERIFY;

    /* A reset that came from the firmware misbehaving is evidence about THIS
     * image, and it outranks anything the subsystems go on to report. A
     * brownout is not: on a battery-powered speaker that is the pack, not the
     * code, and rolling back over it would punish the wrong thing. */
    const esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_INT_WDT) {
        ESP_LOGW(TAG, "previous run ended in reset reason %d", (int)reason);
        hk_health_report_fault();
    }

    if (s_pending) {
        ESP_LOGW(TAG, "this image is PENDING VERIFY: it is confirmed or rolled "
                      "back before the next restart");
    } else {
        ESP_LOGI(TAG, "image state %d: nothing to confirm", (int)state);
    }
    return s_pending;
}

void hk_health_monitor_tick(uint32_t now_ms)
{
    if (!s_pending || s_settled) {
        return;
    }

    hk_health_inputs_t snapshot;
    portENTER_CRITICAL(&s_lock);
    snapshot = s_inputs;
    portEXIT_CRITICAL(&s_lock);
    snapshot.uptime_ms = now_ms;

    const hk_health_limits_t limits = {
        .settle_ms = HK_HEALTH_SETTLE_MS_DEFAULT,
        .deadline_ms = HK_HEALTH_DEADLINE_MS_DEFAULT,
    };
    hk_health_reason_t why;
    const hk_health_verdict_t verdict =
        hk_health_evaluate(&snapshot, &limits, &why);

    switch (verdict) {
    case HK_HEALTH_WAIT:
        return;

    case HK_HEALTH_CONFIRM:
        s_settled = true;
        ESP_LOGI(TAG, "confirming this image after %" PRIu32 " ms", now_ms);
        if (s_persist != NULL) {
            s_persist(true);
        }
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
            ESP_LOGE(TAG, "could not confirm the image; it will roll back");
        }
        return;

    case HK_HEALTH_ROLLBACK:
        s_settled = true;
        ESP_LOGE(TAG, "rolling back: %s", hk_health_reason_name(why));
        /* Before the reboot, not after: there is no after. */
        if (s_persist != NULL) {
            s_persist(false);
        }
        /* Does not return: it reboots into the previous slot. */
        (void)esp_ota_mark_app_invalid_rollback_and_reboot();
        ESP_LOGE(TAG, "rollback refused; the previous image may be missing");
        return;
    }
}
