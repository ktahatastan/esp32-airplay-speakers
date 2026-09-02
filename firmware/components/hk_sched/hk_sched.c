#include "hk_sched.h"

#include <stddef.h>

/*
 * The signed-difference comparison below is valid while every interval stays
 * under half the counter's range. That is where the limit comes from: it is
 * not a policy choice, it is the arithmetic.
 */
#define HK_SCHED_MAX_INTERVAL_MS 0x7FFFFFFFu

bool hk_sched_limits_sane(const hk_sched_limits_t *limits)
{
    if (limits == NULL) {
        return false;
    }
    /* A zero interval asks the same question forever. */
    if (limits->interval_ms == 0u) {
        return false;
    }
    /* A ceiling below the starting backoff means the doubling never runs and
     * every failure waits the same short time — the opposite of backing off. */
    if (limits->backoff_max_ms < limits->backoff_ms || limits->backoff_ms == 0u) {
        return false;
    }
    if (limits->interval_ms > HK_SCHED_MAX_INTERVAL_MS ||
        limits->first_delay_ms > HK_SCHED_MAX_INTERVAL_MS ||
        limits->jitter_ms > HK_SCHED_MAX_INTERVAL_MS ||
        limits->backoff_max_ms > HK_SCHED_MAX_INTERVAL_MS) {
        return false;
    }
    /* An interval plus its jitter must still fit, or the sum wraps into the
     * past and every check becomes immediately due. */
    if (limits->interval_ms > HK_SCHED_MAX_INTERVAL_MS - limits->jitter_ms) {
        return false;
    }
    /* The same sum is formed on the failure path, and feeds the same signed
     * comparison. Guarding only the interval left a configuration this
     * function called sane that could still produce a due time the comparison
     * reads as already past. */
    if (limits->backoff_max_ms > HK_SCHED_MAX_INTERVAL_MS - limits->jitter_ms) {
        return false;
    }
    return true;
}

/** A value in [0, span). Zero span yields zero rather than dividing by it. */
static uint32_t spread(uint32_t random, uint32_t span)
{
    return (span == 0u) ? 0u : (random % span);
}

void hk_sched_init(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                   const hk_sched_limits_t *limits)
{
    if (sched == NULL) {
        return;
    }
    sched->failures = 0u;
    sched->armed = false;
    sched->due_ms = now_ms;

    if (!hk_sched_limits_sane(limits)) {
        return;
    }

    /* The first check is spread across the whole window rather than placed at
     * the nominal interval. Four speakers that came back from the same power
     * cut are otherwise still synchronised a day later, and the day after. */
    sched->due_ms = now_ms + spread(random, limits->first_delay_ms);
    sched->armed = true;
}

bool hk_sched_due(const hk_sched_t *sched, uint32_t now_ms)
{
    if (sched == NULL || !sched->armed) {
        return false;
    }
    /* Signed difference on the unsigned counter: correct across the 32-bit
     * wrap, which a plain `now >= due` is not. */
    return (int32_t)(now_ms - sched->due_ms) >= 0;
}

uint32_t hk_sched_remaining(const hk_sched_t *sched, uint32_t now_ms)
{
    if (sched == NULL || !sched->armed) {
        return 0u;
    }
    const int32_t delta = (int32_t)(sched->due_ms - now_ms);
    return (delta <= 0) ? 0u : (uint32_t)delta;
}

void hk_sched_success(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                      const hk_sched_limits_t *limits)
{
    if (sched == NULL || !hk_sched_limits_sane(limits)) {
        return;
    }
    sched->failures = 0u;
    sched->due_ms = now_ms + limits->interval_ms + spread(random, limits->jitter_ms);
    sched->armed = true;
}

void hk_sched_failure(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                      const hk_sched_limits_t *limits)
{
    if (sched == NULL || !hk_sched_limits_sane(limits)) {
        return;
    }
    if (sched->failures < 255u) {
        sched->failures++;
    }

    /* Double per failure, up to the ceiling. The shift is bounded before it is
     * taken: shifting a uint32_t by 32 or more is undefined, and a device that
     * has been offline for a while would reach that quickly. */
    uint32_t wait = limits->backoff_ms;
    for (uint8_t i = 1u; i < sched->failures; i++) {
        if (wait >= limits->backoff_max_ms || wait > HK_SCHED_MAX_INTERVAL_MS / 2u) {
            break;
        }
        wait *= 2u;
    }
    if (wait > limits->backoff_max_ms) {
        wait = limits->backoff_max_ms;
    }

    /* Jitter on the retry too. Four devices that lost the same network
     * recover together otherwise, and hit the server together the moment it
     * comes back. */
    sched->due_ms = now_ms + wait + spread(random, limits->jitter_ms);
    sched->armed = true;
}
