/**
 * @file hk_sched.h
 * @brief When to look for an update, and how to back off when looking fails.
 *
 * Four speakers on one network share one uplink and one GitHub. The scheduler
 * exists so they do not behave like one device with four times the appetite.
 *
 * THE RANDOM DELAY IS THE POINT, not a refinement. Four speakers on the same
 * mains circuit come back from a power cut within milliseconds of each other,
 * join the same Wi-Fi at the same moment and would, without it, ask the same
 * server the same question simultaneously — forever, every day, at whatever
 * time the last power cut happened. Spreading the first check is what stops a
 * household from looking like a small denial of service.
 *
 * BACKOFF CAPS RATHER THAN GIVES UP. A device that stops checking is a device
 * that can never be fixed remotely, which is the opposite of what an update
 * mechanism is for. Repeated failures slow the checks down to a floor and stay
 * there; the network comes back eventually and so does the speaker.
 *
 * RANDOMNESS IS INJECTED, like every threshold in this project. The caller
 * passes a number; this module never calls a generator. That keeps the whole
 * schedule reproducible in a test — including the awkward cases around the
 * 32-bit millisecond wrap, which are otherwise almost impossible to reach.
 */
#ifndef HK_SCHED_H
#define HK_SCHED_H

#include <stdbool.h>
#include <stdint.h>

/** Timings. No defaults: see hk_sched_limits_sane(). */
typedef struct {
    uint32_t interval_ms;     /**< Nominal gap between checks */
    uint32_t first_delay_ms;  /**< Longest random wait before the very first check */
    uint32_t jitter_ms;       /**< Random spread added to each later check */
    uint32_t backoff_ms;      /**< Wait after the first failure */
    uint32_t backoff_max_ms;  /**< Ceiling the backoff doubles up to */
} hk_sched_limits_t;

/** Scheduler state, owned by the caller. */
typedef struct {
    uint32_t due_ms;      /**< When the next check may happen */
    uint8_t  failures;    /**< Consecutive failures, saturating */
    bool     armed;       /**< A check has been scheduled */
} hk_sched_t;

/**
 * Whether the timings can produce a working schedule.
 *
 * An interval of zero asks a server a question continuously. A backoff ceiling
 * below the backoff start means the first failure already exceeds the cap and
 * the doubling never happens. Both are refused where they are written.
 *
 * Every interval must also stay under about 24 days, because the due-time
 * comparison is a signed difference on a 32-bit millisecond counter and
 * anything longer becomes ambiguous with the past.
 */
bool hk_sched_limits_sane(const hk_sched_limits_t *limits);

/**
 * Arm the first check.
 *
 * @param random any number; only its remainder is used, so a weak source is
 *               fine — the goal is that four devices differ, not secrecy.
 */
void hk_sched_init(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                   const hk_sched_limits_t *limits);

/** Whether a check is due. False whenever the schedule is unusable. */
bool hk_sched_due(const hk_sched_t *sched, uint32_t now_ms);

/** Record a completed check — found something or not — and schedule the next. */
void hk_sched_success(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                      const hk_sched_limits_t *limits);

/** Record a failed check and schedule a retry, further out each time. */
void hk_sched_failure(hk_sched_t *sched, uint32_t now_ms, uint32_t random,
                      const hk_sched_limits_t *limits);

/** Milliseconds until the next check, or 0 if one is due now. */
uint32_t hk_sched_remaining(const hk_sched_t *sched, uint32_t now_ms);

#endif /* HK_SCHED_H */
