#include "hk_test.h"
#include "hk_health.h"

#include <string.h>

/*
 * The two failures this module exists to prevent pull in opposite directions,
 * so both are tested rather than just the tidy one:
 *
 *   confirming a broken image  -> the speaker is stuck until someone finds a
 *                                 USB cable
 *   never confirming a good one -> updates silently stop sticking, with no
 *                                 error anywhere to explain why
 *
 * Most of these cases are about the second. It is the one that looks like
 * nothing is wrong.
 */

static hk_health_limits_t limits(void)
{
    hk_health_limits_t l = {
        .settle_ms = HK_HEALTH_SETTLE_MS_DEFAULT,
        .deadline_ms = HK_HEALTH_DEADLINE_MS_DEFAULT,
    };
    return l;
}

/** Everything reported, everything good, long enough ago to confirm. */
static hk_health_inputs_t healthy(void)
{
    hk_health_inputs_t i = {
        .storage = HK_HEALTH_PASS,
        .network = HK_HEALTH_PASS,
        .audio = HK_HEALTH_PASS,
        .telemetry = HK_HEALTH_PASS,
        .uptime_ms = 31000u,
        .critical_fault = false,
    };
    return i;
}

static hk_health_verdict_t judge(hk_health_inputs_t in, hk_health_reason_t *why)
{
    hk_health_limits_t lim = limits();
    return hk_health_evaluate(&in, &lim, why);
}

void test_health(void)
{
    hk_health_limits_t lim = limits();
    hk_health_inputs_t in;
    hk_health_reason_t why = HK_HEALTH_REASON_OK;

    /* --- a good image, confirmed --- */
    HK_CHECK_EQ_INT(judge(healthy(), &why), HK_HEALTH_CONFIRM);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_OK);

    /* --- the settle time is a floor, not a suggestion --- */
    in = healthy();
    in.uptime_ms = 29999u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_SETTLING);

    /* Exactly at the boundary confirms: the plan asks for at least 30 s. */
    in = healthy();
    in.uptime_ms = 30000u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_CONFIRM);

    /* --- an explicit failure rolls back at once, without serving out the
     * settle time. Waiting would only keep a broken speaker playing badly. --- */
    in = healthy();
    in.audio = HK_HEALTH_FAIL;
    in.uptime_ms = 100u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_AUDIO_FAILED);

    in = healthy();
    in.storage = HK_HEALTH_FAIL;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_STORAGE_FAILED);

    in = healthy();
    in.telemetry = HK_HEALTH_FAIL;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_TELEMETRY_FAILED);

    /* --- a recorded panic outranks every subsystem's own opinion --- */
    in = healthy();
    in.critical_fault = true;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_CRITICAL_FAULT);

    /* Even when it happens before anything has reported. */
    in = (hk_health_inputs_t){0};
    in.critical_fault = true;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_CRITICAL_FAULT);

    /* --- silence is patience, not failure ---
     * A subsystem that has not reported yet must not roll the image back.
     * Treating silence as failure would revert every image that merely
     * booted slowly, which is the hardest kind of bug to see. */
    in = healthy();
    in.audio = HK_HEALTH_UNKNOWN;
    in.uptime_ms = 500u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_INCOMPLETE);

    /* Still waiting well past the settle time, because a criterion is open. */
    in = healthy();
    in.audio = HK_HEALTH_UNKNOWN;
    in.uptime_ms = 60000u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_INCOMPLETE);

    /* --- but silence at the deadline is a verdict ---
     * By now it is not slow, it is absent. */
    in = healthy();
    in.audio = HK_HEALTH_UNKNOWN;
    in.uptime_ms = HK_HEALTH_DEADLINE_MS_DEFAULT;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_AUDIO_SILENT);

    /* One millisecond earlier it is still waiting. */
    in = healthy();
    in.audio = HK_HEALTH_UNKNOWN;
    in.uptime_ms = HK_HEALTH_DEADLINE_MS_DEFAULT - 1u;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);

    /* Each silent subsystem names itself. */
    in = healthy();
    in.telemetry = HK_HEALTH_UNKNOWN;
    in.uptime_ms = HK_HEALTH_DEADLINE_MS_DEFAULT;
    HK_CHECK_EQ_INT(why = HK_HEALTH_REASON_OK, HK_HEALTH_REASON_OK);
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_TELEMETRY_SILENT);

    /* --- a criterion the build does not have is satisfied, not pending ---
     * A prototype with no NTC fitted must still be able to confirm, and it
     * must have to say so explicitly rather than get there by accident. */
    in = healthy();
    in.telemetry = HK_HEALTH_SKIP;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_CONFIRM);

    in = healthy();
    in.audio = HK_HEALTH_SKIP;
    in.telemetry = HK_HEALTH_SKIP;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_CONFIRM);

    /* --- but the check cannot be switched off ---
     * Skipping everything would confirm every image unconditionally: a
     * rollback that is not a rollback, and that still looks like one in the
     * log. storage and network belong to components present in every build,
     * so declaring them inapplicable is refused. */
    in = healthy();
    in.storage = HK_HEALTH_SKIP;
    in.network = HK_HEALTH_SKIP;
    in.audio = HK_HEALTH_SKIP;
    in.telemetry = HK_HEALTH_SKIP;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_UNSKIPPABLE);

    in = healthy();
    in.storage = HK_HEALTH_SKIP;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_UNSKIPPABLE);

    in = healthy();
    in.network = HK_HEALTH_SKIP;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_UNSKIPPABLE);

    HK_CHECK(strcmp(hk_health_reason_name(HK_HEALTH_REASON_UNSKIPPABLE), "UNSKIPPABLE") == 0);

    /* --- the case that would roll back a perfectly good image ---
     * The owner changed their Wi-Fi password. The device cannot join, opens
     * provisioning on purpose, and reports that as PASS. Reverting here would
     * be wrong twice over: the firmware is fine, and the previous image cannot
     * connect either, so it would revert and revert again. */
    in = healthy();
    in.network = HK_HEALTH_PASS;   /* provisioning open, deliberately */
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_CONFIRM);

    /* Only the network stack failing to start is a real failure. */
    in = healthy();
    in.network = HK_HEALTH_FAIL;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_NETWORK_FAILED);

    /* --- ordering: decisiveness first --- */
    /* A failure beats a silence, whoever is listed first. */
    in = healthy();
    in.storage = HK_HEALTH_UNKNOWN;
    in.audio = HK_HEALTH_FAIL;
    in.uptime_ms = HK_HEALTH_DEADLINE_MS_DEFAULT;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_AUDIO_FAILED);

    /* A fault beats a failure. */
    in = healthy();
    in.audio = HK_HEALTH_FAIL;
    in.critical_fault = true;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_CRITICAL_FAULT);

    /* Among several silences, the first criterion is the one reported. */
    in = healthy();
    in.storage = HK_HEALTH_UNKNOWN;
    in.network = HK_HEALTH_UNKNOWN;
    in.uptime_ms = HK_HEALTH_DEADLINE_MS_DEFAULT;
    HK_CHECK_EQ_INT(judge(in, &why), HK_HEALTH_ROLLBACK);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_STORAGE_SILENT);

    /* --- missing information never confirms --- */
    in = healthy();
    HK_CHECK_EQ_INT(hk_health_evaluate(NULL, &lim, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_ARG);
    HK_CHECK_EQ_INT(hk_health_evaluate(&in, NULL, &why), HK_HEALTH_WAIT);
    HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_ARG);

    /* The reason pointer is optional. */
    HK_CHECK_EQ_INT(hk_health_evaluate(&in, &lim, NULL), HK_HEALTH_CONFIRM);

    /* --- a deadline that does not leave room to settle is a config bug ---
     * Left alone it would look like a subsystem problem: every image would
     * reach the deadline before it was ever allowed to confirm, and the log
     * would blame whichever criterion happened to be slowest. */
    {
        hk_health_limits_t bad = {.settle_ms = 30000u, .deadline_ms = 30000u};
        hk_health_inputs_t ok = healthy();
        HK_CHECK_EQ_INT(hk_health_evaluate(&ok, &bad, &why), HK_HEALTH_WAIT);
        HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_BAD_LIMITS);

        bad.deadline_ms = 10000u;
        HK_CHECK_EQ_INT(hk_health_evaluate(&ok, &bad, &why), HK_HEALTH_WAIT);
        HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_BAD_LIMITS);

        /* One millisecond of room is enough to be legal. */
        bad.deadline_ms = 30001u;
        HK_CHECK_EQ_INT(hk_health_evaluate(&ok, &bad, &why), HK_HEALTH_CONFIRM);
    }

    /* Bad limits must not mask a fault that already happened. Rolling back a
     * panicking image matters more than a tidy configuration error... but the
     * limits are checked first on purpose, because a build that cannot judge
     * itself should not be making rollback decisions either. Pinned so the
     * choice is deliberate rather than incidental. */
    {
        hk_health_limits_t bad = {.settle_ms = 30000u, .deadline_ms = 1000u};
        hk_health_inputs_t faulted = healthy();
        faulted.critical_fault = true;
        HK_CHECK_EQ_INT(hk_health_evaluate(&faulted, &bad, &why), HK_HEALTH_WAIT);
        HK_CHECK_EQ_INT(why, HK_HEALTH_REASON_BAD_LIMITS);
    }

    /* --- names, so a boot log is readable --- */
    HK_CHECK(strcmp(hk_health_verdict_name(HK_HEALTH_CONFIRM), "CONFIRM") == 0);
    HK_CHECK(strcmp(hk_health_verdict_name(HK_HEALTH_ROLLBACK), "ROLLBACK") == 0);
    HK_CHECK(strcmp(hk_health_verdict_name((hk_health_verdict_t)99), "UNKNOWN") == 0);
    HK_CHECK(strcmp(hk_health_reason_name(HK_HEALTH_REASON_AUDIO_SILENT), "AUDIO_SILENT") == 0);
    HK_CHECK(strcmp(hk_health_reason_name(HK_HEALTH_REASON_BAD_LIMITS), "BAD_LIMITS") == 0);
    HK_CHECK(strcmp(hk_health_reason_name((hk_health_reason_t)99), "UNKNOWN") == 0);
}
