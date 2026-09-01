#include "hk_test.h"
#include "hk_sched.h"

/*
 * Stand-ins. A real deployment would use a day and an hour; these are shrunk
 * so a test can walk a year of schedule in a loop.
 */
static hk_sched_limits_t limits(void)
{
    hk_sched_limits_t l = {
        .interval_ms = 86400000u,   /* a day */
        .first_delay_ms = 3600000u, /* up to an hour */
        .jitter_ms = 600000u,       /* ten minutes */
        .backoff_ms = 60000u,       /* a minute */
        .backoff_max_ms = 3600000u, /* an hour */
    };
    return l;
}

void test_sched(void)
{
    hk_sched_limits_t lim = limits();
    hk_sched_t s;

    /* ===== timings that cannot produce a schedule ===== */
    HK_CHECK(hk_sched_limits_sane(&lim));
    HK_CHECK(!hk_sched_limits_sane(NULL));
    {
        hk_sched_limits_t bad;
        bad = limits(); bad.interval_ms = 0;              /* asks forever */
        HK_CHECK(!hk_sched_limits_sane(&bad));
        bad = limits(); bad.backoff_ms = 0;
        HK_CHECK(!hk_sched_limits_sane(&bad));
        bad = limits(); bad.backoff_max_ms = bad.backoff_ms - 1u;  /* never doubles */
        HK_CHECK(!hk_sched_limits_sane(&bad));
        bad = limits(); bad.interval_ms = 0x7FFFFFFFu;    /* interval + jitter wraps */
        HK_CHECK(!hk_sched_limits_sane(&bad));
        bad = limits(); bad.interval_ms = 0x80000000u;    /* past the signed range */
        HK_CHECK(!hk_sched_limits_sane(&bad));

        /* and an unusable schedule never reports a check as due */
        bad = limits(); bad.interval_ms = 0;
        hk_sched_init(&s, 1000u, 12345u, &bad);
        HK_CHECK(!s.armed);
        HK_CHECK(!hk_sched_due(&s, 0xFFFFFFFFu));
    }

    /* ===== the first check is spread, not immediate ===== */
    /* This is the whole reason the module exists: four speakers coming back
     * from one power cut must not ask the same server at the same instant. */
    {
        uint32_t earliest = 0xFFFFFFFFu, latest = 0u;
        for (uint32_t r = 0; r < 4000u; r++) {
            hk_sched_init(&s, 0u, r * 7919u, &lim);   /* a prime, to scatter */
            HK_CHECK(s.armed);
            const uint32_t wait = hk_sched_remaining(&s, 0u);
            HK_CHECK(wait < lim.first_delay_ms);
            if (wait < earliest) { earliest = wait; }
            if (wait > latest) { latest = wait; }
        }
        /* Four devices really do land in different places: the spread has to
         * cover most of the window, not cluster. */
        HK_CHECK(latest - earliest > lim.first_delay_ms / 2u);
    }

    /* ===== a check is not due before its time ===== */
    hk_sched_init(&s, 1000u, 0u, &lim);          /* random 0: due immediately */
    HK_CHECK(hk_sched_due(&s, 1000u));
    hk_sched_init(&s, 1000u, 500000u, &lim);
    HK_CHECK(!hk_sched_due(&s, 1000u));
    HK_CHECK(hk_sched_due(&s, 1000u + 500000u));
    HK_CHECK(hk_sched_due(&s, 1000u + 900000u));

    /* ===== a successful check schedules the next one a day out ===== */
    hk_sched_success(&s, 5000u, 0u, &lim);
    HK_CHECK_EQ_INT(s.failures, 0);
    HK_CHECK(!hk_sched_due(&s, 5000u + lim.interval_ms - 1u));
    HK_CHECK(hk_sched_due(&s, 5000u + lim.interval_ms));
    /* never more than a day plus the jitter */
    for (uint32_t r = 0; r < 1000u; r++) {
        hk_sched_success(&s, 0u, r * 104729u, &lim);
        const uint32_t wait = hk_sched_remaining(&s, 0u);
        HK_CHECK(wait >= lim.interval_ms);
        HK_CHECK(wait < lim.interval_ms + lim.jitter_ms);
    }

    /* ===== failures back off, and the backoff doubles ===== */
    hk_sched_init(&s, 0u, 0u, &lim);
    hk_sched_failure(&s, 0u, 0u, &lim);
    HK_CHECK_EQ_INT(s.failures, 1);
    HK_CHECK_EQ_INT(hk_sched_remaining(&s, 0u), lim.backoff_ms);

    hk_sched_failure(&s, 0u, 0u, &lim);
    HK_CHECK_EQ_INT(hk_sched_remaining(&s, 0u), lim.backoff_ms * 2u);
    hk_sched_failure(&s, 0u, 0u, &lim);
    HK_CHECK_EQ_INT(hk_sched_remaining(&s, 0u), lim.backoff_ms * 4u);

    /* ===== and it caps rather than giving up =====
     * A device that stops checking can never be fixed remotely, which is the
     * opposite of what an update mechanism is for. */
    for (int i = 0; i < 400; i++) {
        hk_sched_failure(&s, 0u, 0u, &lim);
        const uint32_t wait = hk_sched_remaining(&s, 0u);
        HK_CHECK(wait <= lim.backoff_max_ms);
        HK_CHECK(s.armed);          /* still going */
    }
    HK_CHECK_EQ_INT(hk_sched_remaining(&s, 0u), lim.backoff_max_ms);
    HK_CHECK(hk_sched_due(&s, lim.backoff_max_ms));

    /* The counter saturates rather than wrapping. Wrapping to zero would make
     * a device that has been offline for a week suddenly behave as though the
     * network had just come back, and retry as fast as it did on the first
     * failure. */
    HK_CHECK_EQ_INT(s.failures, 255);

    /* one success clears it all */
    hk_sched_success(&s, 0u, 0u, &lim);
    HK_CHECK_EQ_INT(s.failures, 0);
    HK_CHECK_EQ_INT(hk_sched_remaining(&s, 0u), lim.interval_ms);

    /* ===== retries are spread too ===== */
    /* Four devices that lost the same network recover together and would hit
     * the server together the moment it returns. */
    {
        uint32_t earliest = 0xFFFFFFFFu, latest = 0u;
        for (uint32_t r = 0; r < 2000u; r++) {
            hk_sched_init(&s, 0u, 0u, &lim);
            hk_sched_failure(&s, 0u, r * 7919u, &lim);
            const uint32_t wait = hk_sched_remaining(&s, 0u);
            if (wait < earliest) { earliest = wait; }
            if (wait > latest) { latest = wait; }
        }
        HK_CHECK(latest - earliest > lim.jitter_ms / 2u);
    }

    /* ===== the millisecond counter wraps =====
     * A speaker left on for 49.7 days must not decide that every check is
     * overdue, nor that none ever is. A plain `now >= due` gets both wrong. */
    {
        const uint32_t near_wrap = 0xFFFFF000u;
        hk_sched_init(&s, near_wrap, 0u, &lim);
        hk_sched_success(&s, near_wrap, 0u, &lim);   /* due a day later, past the wrap */

        HK_CHECK(!hk_sched_due(&s, near_wrap));
        HK_CHECK(!hk_sched_due(&s, 0x00000010u));    /* wrapped, but not yet due */
        HK_CHECK(!hk_sched_due(&s, near_wrap + lim.interval_ms - 1000u));
        HK_CHECK(hk_sched_due(&s, near_wrap + lim.interval_ms));
        HK_CHECK(hk_sched_due(&s, near_wrap + lim.interval_ms + 5000u));

        /* and remaining() stays sane across it */
        const uint32_t left = hk_sched_remaining(&s, 0x00000010u);
        HK_CHECK(left > 0u && left <= lim.interval_ms);
    }

    /* ===== a year of schedule, with nothing drifting or stalling ===== */
    {
        hk_sched_init(&s, 0u, 12345u, &lim);
        uint32_t now = 0u;
        int checks = 0;
        for (int day = 0; day < 365; day++) {
            /* walk to the due time */
            now += hk_sched_remaining(&s, now);
            HK_CHECK(hk_sched_due(&s, now));
            checks++;
            hk_sched_success(&s, now, (uint32_t)day * 2654435761u, &lim);
            HK_CHECK(!hk_sched_due(&s, now));   /* not immediately due again */
        }
        HK_CHECK_EQ_INT(checks, 365);
    }

    /* ===== degenerate calls ===== */
    HK_CHECK(!hk_sched_due(NULL, 0u));
    HK_CHECK_EQ_INT(hk_sched_remaining(NULL, 0u), 0);
    hk_sched_init(NULL, 0u, 0u, &lim);
    hk_sched_success(NULL, 0u, 0u, &lim);
    hk_sched_failure(NULL, 0u, 0u, &lim);
    hk_sched_init(&s, 0u, 0u, NULL);
    HK_CHECK(!s.armed);
}
