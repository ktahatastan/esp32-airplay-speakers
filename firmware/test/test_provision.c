#include "hk_test.h"
#include "hk_provision.h"

/*
 * docs/controls-and-provisioning-plan.md and ADR-0005.
 *
 * The interesting cases are the asymmetries: first boot must not time out,
 * a deliberately opened window must, the radios have to survive the connection
 * attempt that immediately follows provisioning, and a short press means two
 * different things depending on whether there is a working network to lose.
 */
void test_provision(void)
{
    hk_prov_t prov;
    uint32_t t = 10000u;

    /* --- first boot, nothing stored --- */
    hk_prov_init(&prov, false, false, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(hk_prov_radios(&prov).ble);
    HK_CHECK(hk_prov_radios(&prov).softap);
    HK_CHECK(!hk_prov_ble_releasable(&prov));

    /* It must still be open long after any window would have expired: a user
     * who walks away mid-setup should come back to a device they can finish. */
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS * 3u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* --- credentials arrive: radios stay up across the attempt --- */
    hk_prov_handle(&prov, HK_PROV_EV_CREDENTIALS, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    HK_CHECK(hk_prov_radios(&prov).softap);  /* the phone is still attached */

    /* Wrong password: back to setup, not a silent retry. */
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* Right password: everything shuts and BLE can be freed. */
    hk_prov_handle(&prov, HK_PROV_EV_CREDENTIALS, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_ONLINE);
    HK_CHECK(!hk_prov_radios(&prov).ble);
    HK_CHECK(!hk_prov_radios(&prov).softap);
    HK_CHECK(hk_prov_ble_releasable(&prov));
    HK_CHECK_EQ_INT(prov.consecutive_failures, 0);

    /* --- a window opened on a configured device does expire --- */
    /* Two presses, because the speaker is ONLINE; the gate below is what makes
     * that necessary, and this block is about what happens after it opens. */
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(prov.bounded);
    HK_CHECK(hk_prov_radios(&prov).ble);

    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS - 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);   /* not yet */
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);     /* now */
    HK_CHECK(!hk_prov_radios(&prov).ble);
    HK_CHECK(hk_prov_ble_releasable(&prov));

    /* Pressing again while a window is open extends it rather than cutting
     * off a user who is still typing. That single refreshing press is
     * deliberately NOT gated: the window is already open, so it costs nothing,
     * and asking a user mid-setup to press twice to keep the page alive would
     * be the confirmation appearing where it does no good. */
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS - 1000u);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_WINDOW_MS - 1000u);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS + 1000u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);   /* refreshed, still open */
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* --- the confirmation gate on a working speaker --- */
    /*
     * The defect this exists for, seen on the bench: one accidental tap on a
     * configured, connected, playing speaker took it off the house network for
     * up to HK_PROV_WINDOW_MS and stopped the music. Opening setup is not
     * passive -- hk_network puts the station down so the provisioning manager
     * can scan -- so the press that opens it has to be asked for twice.
     *
     * ONLY from ONLINE. Everywhere else there is no working join to lose, and
     * a second press would be friction bought with nothing.
     */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);

    /* One press: nothing a listener could hear, and nothing a phone could see. */
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_ONLINE);
    HK_CHECK(!hk_prov_radios(&prov).ble);
    HK_CHECK(!hk_prov_radios(&prov).softap);
    /* But it is not silent either. A gate the user cannot see is a device that
     * ignores them, so the armed state has to be readable. */
    HK_CHECK(hk_prov_confirm_pending(&prov, t));

    /* The second press, inside the confirmation window, opens exactly what a
     * single press opened before: a bounded window with both radios up. */
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_CONFIRM_MS - 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(prov.bounded);
    HK_CHECK(hk_prov_radios(&prov).ble);
    HK_CHECK(hk_prov_radios(&prov).softap);
    /* Confirmed and spent: the prompt must not still be showing. */
    HK_CHECK(!hk_prov_confirm_pending(&prov, t + HK_PROV_CONFIRM_MS - 1u));

    /* An arm nobody confirms expires by itself, and the next tick clears the
     * prompt with it rather than leaving a stale "press again" on the screen. */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK(hk_prov_confirm_pending(&prov, t + HK_PROV_CONFIRM_MS - 1u));
    HK_CHECK(!hk_prov_confirm_pending(&prov, t + HK_PROV_CONFIRM_MS));
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_CONFIRM_MS);
    HK_CHECK(!prov.confirm_armed);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_ONLINE);   /* the tick opened nothing */
    HK_CHECK(!hk_prov_radios(&prov).ble);

    /* Two taps far enough apart are two accidents, not one decision. No tick
     * runs between them on purpose: the press itself has to check the deadline
     * rather than trust that something else cleared the flag first. */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_CONFIRM_MS);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_ONLINE);   /* too late to be a confirmation */
    HK_CHECK(!hk_prov_radios(&prov).ble);
    /* It re-arms instead of being thrown away, or a user whose second press was
     * a moment slow would have to press three times. */
    HK_CHECK(hk_prov_confirm_pending(&prov, t + HK_PROV_CONFIRM_MS));
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_CONFIRM_MS + 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* Long after the arm, with no tick to tidy up: still just an arm. */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_CONFIRM_MS * 100u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_ONLINE);
    HK_CHECK(!hk_prov_radios(&prov).ble);

    /* The confirmation deadline crosses the 32-bit wrap, like every other
     * deadline in this module. */
    hk_prov_init(&prov, true, false, 0xFFFFFF00u);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, 0xFFFFFF00u);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, 0xFFFFFF00u);
    HK_CHECK(hk_prov_confirm_pending(&prov, 0xFFFFFF00u + HK_PROV_CONFIRM_MS - 1u));
    HK_CHECK(!hk_prov_confirm_pending(&prov, 0xFFFFFF00u + HK_PROV_CONFIRM_MS));
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, 0xFFFFFF00u + HK_PROV_CONFIRM_MS - 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);

    /* --- the arm does not leak into the holds --- */
    /*
     * A short press followed by a 5 s or 12 s hold is one gesture the user
     * changed their mind about partway through. The hold must do exactly what
     * PRD-005 says it does, and the press before it must not turn into half a
     * confirmation for anything.
     */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_NETWORK_RESET, t + 1000u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.bounded);            /* unbounded, exactly as before */
    HK_CHECK(!prov.has_credentials);
    HK_CHECK(hk_prov_radios(&prov).ble);
    HK_CHECK(!hk_prov_confirm_pending(&prov, t + 1000u));

    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_FACTORY_RESET, t + 1000u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.bounded);
    HK_CHECK(!prov.has_credentials);
    HK_CHECK(!hk_prov_confirm_pending(&prov, t + 1000u));

    /* --- states with nothing to lose still act on the FIRST press --- */

    /* Nothing stored: setup is already open, and one press leaves it open and
     * unbounded. No arm is taken, because the press costs nothing here. */
    hk_prov_init(&prov, false, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(hk_prov_radios(&prov).ble);
    HK_CHECK(hk_prov_radios(&prov).softap);
    HK_CHECK(!prov.bounded);
    HK_CHECK(!hk_prov_confirm_pending(&prov, t));

    /* Still trying to join: one press opens, as it always did. Nothing is
     * playing and no station is up, so there is nothing a second press would
     * protect -- and a user whose speaker cannot get onto the network is
     * exactly the user who should not have to guess at a second press. */
    hk_prov_init(&prov, true, false, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(prov.bounded);
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* The failure fallback is open and unbounded; a press refreshes nothing and
     * closes nothing. */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.bounded);
    HK_CHECK(hk_prov_radios(&prov).ble);

    /* --- normal boot with stored credentials --- */
    hk_prov_init(&prov, true, false, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    /* No setup radios: an already-configured speaker must not advertise an
     * open access point every time it powers on. */
    HK_CHECK(!hk_prov_radios(&prov).ble);
    HK_CHECK(!hk_prov_radios(&prov).softap);

    /* A transient failure is retried quietly. */
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    HK_CHECK(!hk_prov_radios(&prov).ble);

    /* But repeated failure means the stored credentials are wrong, and the
     * user needs a way back in. */
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.consecutive_failures, HK_PROV_MAX_FAILURES);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.bounded);   /* unbounded: there is no working network to go back to */

    /* A success resets the counter, so a flaky router does not eventually
     * push a working speaker into setup mode. */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    HK_CHECK_EQ_INT(prov.consecutive_failures, 0);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_FAIL, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);

    /* --- recovery: button held through boot --- */

    /* On a configured device the recovery window is bounded like any other
     * deliberate one. Leaving it open would advertise BLE and an access point
     * for the whole session, never release the BLE stack, and never attempt to
     * join, so the speaker would stay silent until power-cycled. */
    hk_prov_init(&prov, true, true, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(prov.bounded);
    HK_CHECK(prov.has_credentials);  /* recovery opens a door; it erases nothing */
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);
    HK_CHECK(hk_prov_ble_releasable(&prov));

    /* With no credentials there is nothing to fall back to, so recovery must
     * not expire. */
    hk_prov_init(&prov, false, true, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.bounded);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS * 5u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);

    /* --- the two resets --- */
    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_NETWORK_RESET, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.has_credentials);
    HK_CHECK(!prov.bounded);

    hk_prov_init(&prov, true, false, t);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_FACTORY_RESET, t);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    HK_CHECK(!prov.has_credentials);

    /* --- the millisecond counter wraps every 49 days --- */
    uint32_t near_wrap = 0xFFFFFF00u;
    hk_prov_init(&prov, true, false, near_wrap);
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, near_wrap);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, near_wrap);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, near_wrap);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    /* The deadline lands past the wrap; the window must still close on time. */
    hk_prov_handle(&prov, HK_PROV_EV_TICK, near_wrap + HK_PROV_WINDOW_MS - 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, near_wrap + HK_PROV_WINDOW_MS);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);

    HK_CHECK(hk_prov_radios(NULL).ble == false);
    HK_CHECK(hk_prov_confirm_pending(NULL, t) == false);
    HK_CHECK_EQ_STR(hk_prov_state_name(HK_PROV_ONLINE), "online");
}
