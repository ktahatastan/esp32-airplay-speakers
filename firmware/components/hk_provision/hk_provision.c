#include "hk_provision.h"

#include <stddef.h>

static uint32_t elapsed(uint32_t now, uint32_t since)
{
    return now - since;  /* unsigned: correct across the 32-bit wrap */
}

/** Forget a pending confirmation. Any change of state cancels it. */
static void clear_confirm(hk_prov_t *prov)
{
    prov->confirm_armed = false;
}

/** True while a short press has armed the gate and the arm has not expired. */
static bool confirm_pending(const hk_prov_t *prov, uint32_t now_ms)
{
    return prov->confirm_armed &&
           elapsed(now_ms, prov->confirm_armed_ms) < HK_PROV_CONFIRM_MS;
}

/** Open provisioning. `bounded` false means it stays open until setup finishes. */
static void open_provisioning(hk_prov_t *prov, bool bounded, uint32_t now_ms)
{
    prov->state = HK_PROV_PROVISIONING;
    prov->radios_open = true;
    prov->bounded = bounded;
    prov->opened_ms = now_ms;
    /* The window is open; there is nothing left to confirm. */
    clear_confirm(prov);
}

void hk_prov_init(hk_prov_t *prov, bool has_credentials, bool recovery, uint32_t now_ms)
{
    prov->has_credentials = has_credentials;
    prov->consecutive_failures = 0;
    prov->confirm_armed = false;
    prov->confirm_armed_ms = now_ms;

    if (recovery || !has_credentials) {
        /* Two different situations reach this branch, and they need different
         * lifetimes.
         *
         * With no credentials there is nothing to fall back to, so the window
         * must not expire: a device the user has not finished configuring
         * should not go quiet on them.
         *
         * A recovery boot on a device that DOES hold credentials is a window
         * the user opened deliberately, and it is bounded like any other. An
         * unbounded one would leave BLE and an open access point advertising
         * for the whole session, never release the BLE stack (ADR-0005), and
         * never even attempt to join, so the speaker would play nothing until
         * it was power-cycled. On expiry it drops to CONNECTING with the
         * credentials it kept; if that network really is gone, the repeated
         * failure fallback below reopens provisioning unbounded on its own. */
        open_provisioning(prov, has_credentials, now_ms);
        return;
    }

    prov->state = HK_PROV_CONNECTING;
    prov->radios_open = false;
    prov->bounded = false;
    prov->opened_ms = now_ms;
}

void hk_prov_handle(hk_prov_t *prov, hk_prov_event_t event, uint32_t now_ms)
{
    switch (event) {
    case HK_PROV_EV_NETWORK_RESET:
    case HK_PROV_EV_FACTORY_RESET:
        /* Wi-Fi credentials are user settings, so both resets clear them and
         * land in the same place: open, unbounded, waiting to be set up again.
         * Neither touches factory calibration; that lives in its own partition
         * and is not this module's business (PRD-008). */
        prov->has_credentials = false;
        prov->consecutive_failures = 0;
        open_provisioning(prov, false, now_ms);
        return;

    case HK_PROV_EV_BUTTON_SHORT:
        if (prov->state == HK_PROV_PROVISIONING) {
            /* Already open. Refresh a bounded window so a user who is still
             * working does not get cut off; leave an unbounded one alone. */
            if (prov->bounded) {
                prov->opened_ms = now_ms;
            }
            return;
        }

        if (prov->state == HK_PROV_ONLINE) {
            /* The only press that costs something, so the only one that has to
             * be asked for twice.
             *
             * Opening a window is not a passive act on a speaker that is
             * working: hk_network puts the station down so the provisioning
             * manager can run its opening scan, which on a joined, playing
             * device means the music stops and the speaker leaves the house
             * network until the window closes. The owner met that on the bench
             * from one accidental tap. The disconnect itself is correct and
             * stays -- what was missing is a precondition on getting here.
             *
             * So the first press only arms, and the LED and the log say so
             * (hk_prov_confirm_pending). The second press, inside
             * HK_PROV_CONFIRM_MS, opens exactly the window today's code opens.
             * A press that arrives after the arm expired is not a confirmation
             * of anything: it becomes the new first press. */
            if (!confirm_pending(prov, now_ms)) {
                prov->confirm_armed = true;
                prov->confirm_armed_ms = now_ms;
                return;
            }
        }

        /* Confirmed, or a state where nothing is at stake -- CONNECTING has no
         * join to lose and no audio to cut. Deliberately opened on a configured
         * device, so it is bounded. */
        open_provisioning(prov, true, now_ms);
        return;

    case HK_PROV_EV_CREDENTIALS:
        clear_confirm(prov);
        prov->has_credentials = true;
        prov->consecutive_failures = 0;
        prov->state = HK_PROV_CONNECTING;
        /* radios_open is deliberately left as it is. If credentials arrived
         * over the portal or BLE, the phone is still attached and needs to see
         * whether the join succeeded. */
        return;

    case HK_PROV_EV_CONNECT_OK:
        clear_confirm(prov);
        prov->state = HK_PROV_ONLINE;
        prov->radios_open = false;
        prov->bounded = false;
        prov->consecutive_failures = 0;
        return;

    case HK_PROV_EV_CONNECT_FAIL:
        clear_confirm(prov);
        if (prov->consecutive_failures < 0xFFu) {
            prov->consecutive_failures++;
        }
        if (prov->radios_open) {
            /* The user is standing there watching. Go straight back to the
             * setup window instead of retrying silently. */
            open_provisioning(prov, prov->bounded, prov->opened_ms);
            return;
        }
        if (prov->consecutive_failures >= HK_PROV_MAX_FAILURES) {
            /* Stored credentials no longer work: the network changed, or the
             * password did. Fall back to provisioning so there is a way in. */
            open_provisioning(prov, false, now_ms);
            return;
        }
        prov->state = HK_PROV_CONNECTING;
        return;

    case HK_PROV_EV_TICK:
    default:
        break;
    }

    if (prov->confirm_armed && elapsed(now_ms, prov->confirm_armed_ms) >= HK_PROV_CONFIRM_MS) {
        /* Nobody confirmed. Dropping the flag here rather than only inside the
         * press keeps what the LED shows honest: the prompt goes away by
         * itself, within one tick of the arm expiring. */
        clear_confirm(prov);
    }

    if (prov->state == HK_PROV_PROVISIONING && prov->bounded &&
        elapsed(now_ms, prov->opened_ms) >= HK_PROV_WINDOW_MS) {
        /* The window expired. Shut the radios and go back to using the
         * credentials we already had. */
        prov->state = HK_PROV_CONNECTING;
        prov->radios_open = false;
        prov->bounded = false;
    }
}

hk_prov_radios_t hk_prov_radios(const hk_prov_t *prov)
{
    hk_prov_radios_t radios = {false, false};
    if (prov != NULL && prov->radios_open) {
        radios.ble = true;
        radios.softap = true;
    }
    return radios;
}

bool hk_prov_confirm_pending(const hk_prov_t *prov, uint32_t now_ms)
{
    return prov != NULL && confirm_pending(prov, now_ms);
}

bool hk_prov_ble_releasable(const hk_prov_t *prov)
{
    return prov != NULL && !prov->radios_open;
}

const char *hk_prov_state_name(hk_prov_state_t state)
{
    switch (state) {
    case HK_PROV_PROVISIONING: return "provisioning";
    case HK_PROV_CONNECTING:   return "connecting";
    case HK_PROV_ONLINE:       return "online";
    }
    return "unknown";
}
