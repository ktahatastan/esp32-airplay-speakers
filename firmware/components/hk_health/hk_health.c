#include "hk_health.h"

#include <stddef.h>

/** One criterion, paired with what to say when it fails and when it is silent. */
typedef struct {
    hk_health_state_t  state;
    hk_health_reason_t failed;
    hk_health_reason_t silent;
} criterion_t;

static void set_reason(hk_health_reason_t *out, hk_health_reason_t value)
{
    if (out != NULL) {
        *out = value;
    }
}

hk_health_verdict_t hk_health_evaluate(const hk_health_inputs_t *inputs,
                                       const hk_health_limits_t *limits,
                                       hk_health_reason_t *reason)
{
    if (inputs == NULL || limits == NULL) {
        set_reason(reason, HK_HEALTH_REASON_ARG);
        return HK_HEALTH_WAIT;
    }

    /* The deadline has to leave room to confirm. If it does not, every image
     * would hit the deadline before it was ever allowed to settle, and every
     * update would roll back — with the logs showing a subsystem "silent" when
     * the real fault is here in the configuration. */
    if (limits->deadline_ms <= limits->settle_ms) {
        set_reason(reason, HK_HEALTH_REASON_BAD_LIMITS);
        return HK_HEALTH_WAIT;
    }

    /* An all-SKIP health check confirms every image unconditionally: a
     * rollback mechanism switched off while still appearing to work. Two
     * criteria belong to components that are in every build, so skipping them
     * can only be a mistake, and it is refused rather than obeyed. */
    if (inputs->storage == HK_HEALTH_SKIP || inputs->network == HK_HEALTH_SKIP) {
        set_reason(reason, HK_HEALTH_REASON_UNSKIPPABLE);
        return HK_HEALTH_WAIT;
    }

    /* A recorded panic outranks everything else: whatever the subsystems say
     * about themselves, the image has already misbehaved once. */
    if (inputs->critical_fault) {
        set_reason(reason, HK_HEALTH_REASON_CRITICAL_FAULT);
        return HK_HEALTH_ROLLBACK;
    }

    const criterion_t criteria[] = {
        {inputs->storage,   HK_HEALTH_REASON_STORAGE_FAILED,   HK_HEALTH_REASON_STORAGE_SILENT},
        {inputs->network,   HK_HEALTH_REASON_NETWORK_FAILED,   HK_HEALTH_REASON_NETWORK_SILENT},
        {inputs->audio,     HK_HEALTH_REASON_AUDIO_FAILED,     HK_HEALTH_REASON_AUDIO_SILENT},
        {inputs->telemetry, HK_HEALTH_REASON_TELEMETRY_FAILED, HK_HEALTH_REASON_TELEMETRY_SILENT},
    };
    const size_t count = sizeof(criteria) / sizeof(criteria[0]);

    /* An explicit failure is decisive straight away. Waiting out the settle
     * time to reach a conclusion already reached only delays the rollback and
     * leaves the owner listening to a broken speaker for longer. */
    for (size_t i = 0; i < count; i++) {
        if (criteria[i].state == HK_HEALTH_FAIL) {
            set_reason(reason, criteria[i].failed);
            return HK_HEALTH_ROLLBACK;
        }
    }

    /* Silence is patience up to the deadline and a verdict after it. A
     * subsystem that has not reported by now is not slow, it is absent. */
    const bool past_deadline = inputs->uptime_ms >= limits->deadline_ms;
    for (size_t i = 0; i < count; i++) {
        if (criteria[i].state == HK_HEALTH_UNKNOWN) {
            if (past_deadline) {
                set_reason(reason, criteria[i].silent);
                return HK_HEALTH_ROLLBACK;
            }
            set_reason(reason, HK_HEALTH_REASON_INCOMPLETE);
            return HK_HEALTH_WAIT;
        }
    }

    /* Everything that was going to report has reported well. The last
     * criterion is simply time: the plan asks for a stretch of running without
     * a reset before an image is called good. */
    if (inputs->uptime_ms < limits->settle_ms) {
        set_reason(reason, HK_HEALTH_REASON_SETTLING);
        return HK_HEALTH_WAIT;
    }

    set_reason(reason, HK_HEALTH_REASON_OK);
    return HK_HEALTH_CONFIRM;
}

const char *hk_health_verdict_name(hk_health_verdict_t verdict)
{
    switch (verdict) {
    case HK_HEALTH_WAIT:     return "WAIT";
    case HK_HEALTH_CONFIRM:  return "CONFIRM";
    case HK_HEALTH_ROLLBACK: return "ROLLBACK";
    }
    return "UNKNOWN";
}

const char *hk_health_reason_name(hk_health_reason_t reason)
{
    switch (reason) {
    case HK_HEALTH_REASON_OK:                return "OK";
    case HK_HEALTH_REASON_ARG:               return "ARG";
    case HK_HEALTH_REASON_BAD_LIMITS:        return "BAD_LIMITS";
    case HK_HEALTH_REASON_UNSKIPPABLE:       return "UNSKIPPABLE";
    case HK_HEALTH_REASON_CRITICAL_FAULT:    return "CRITICAL_FAULT";
    case HK_HEALTH_REASON_STORAGE_FAILED:    return "STORAGE_FAILED";
    case HK_HEALTH_REASON_NETWORK_FAILED:    return "NETWORK_FAILED";
    case HK_HEALTH_REASON_AUDIO_FAILED:      return "AUDIO_FAILED";
    case HK_HEALTH_REASON_TELEMETRY_FAILED:  return "TELEMETRY_FAILED";
    case HK_HEALTH_REASON_STORAGE_SILENT:    return "STORAGE_SILENT";
    case HK_HEALTH_REASON_NETWORK_SILENT:    return "NETWORK_SILENT";
    case HK_HEALTH_REASON_AUDIO_SILENT:      return "AUDIO_SILENT";
    case HK_HEALTH_REASON_TELEMETRY_SILENT:  return "TELEMETRY_SILENT";
    case HK_HEALTH_REASON_SETTLING:          return "SETTLING";
    case HK_HEALTH_REASON_INCOMPLETE:        return "INCOMPLETE";
    }
    return "UNKNOWN";
}
