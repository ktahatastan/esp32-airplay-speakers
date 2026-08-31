#include "hk_provision.h"

#include <stddef.h>

static uint32_t elapsed(uint32_t now, uint32_t since)
{
    return now - since;  /* unsigned: correct across the 32-bit wrap */
}

/** Open provisioning. `bounded` false means it stays open until setup finishes. */
static void open_provisioning(hk_prov_t *prov, bool bounded, uint32_t now_ms)
{
    prov->state = HK_PROV_PROVISIONING;
    prov->radios_open = true;
    prov->bounded = bounded;
    prov->opened_ms = now_ms;
}

void hk_prov_init(hk_prov_t *prov, bool has_credentials, bool recovery, uint32_t now_ms)
{
    prov->has_credentials = has_credentials;
    prov->consecutive_failures = 0;

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
        } else {
            /* Deliberately opened on a configured device, so it is bounded. */
            open_provisioning(prov, true, now_ms);
        }
        return;

    case HK_PROV_EV_CREDENTIALS:
        prov->has_credentials = true;
        prov->consecutive_failures = 0;
        prov->state = HK_PROV_CONNECTING;
        /* radios_open is deliberately left as it is. If credentials arrived
         * over the portal or BLE, the phone is still attached and needs to see
         * whether the join succeeded. */
        return;

    case HK_PROV_EV_CONNECT_OK:
        prov->state = HK_PROV_ONLINE;
        prov->radios_open = false;
        prov->bounded = false;
        prov->consecutive_failures = 0;
        return;

    case HK_PROV_EV_CONNECT_FAIL:
        if (prov->consecutive_failures < 0xFFu) {
            prov->consecutive_failures++;
        }
        if (prov->radios_open) {
            /* The user is standing there watching. Go straight back to the
             * setup screen instead of retrying silently. */
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
