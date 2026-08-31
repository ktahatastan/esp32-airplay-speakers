#include "hk_test.h"
#include "hk_provision.h"

/*
 * docs/controls-and-provisioning-plan.md and ADR-0005.
 *
 * The interesting cases are the asymmetries: first boot must not time out,
 * a deliberately opened window must, and the radios have to survive the
 * connection attempt that immediately follows provisioning.
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
     * off a user who is still typing. */
    hk_prov_handle(&prov, HK_PROV_EV_CONNECT_OK, t);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS - 1000u);
    hk_prov_handle(&prov, HK_PROV_EV_BUTTON_SHORT, t + HK_PROV_WINDOW_MS - 1000u);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, t + HK_PROV_WINDOW_MS + 1000u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);   /* refreshed, still open */

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
    /* The deadline lands past the wrap; the window must still close on time. */
    hk_prov_handle(&prov, HK_PROV_EV_TICK, near_wrap + HK_PROV_WINDOW_MS - 1u);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_PROVISIONING);
    hk_prov_handle(&prov, HK_PROV_EV_TICK, near_wrap + HK_PROV_WINDOW_MS);
    HK_CHECK_EQ_INT(prov.state, HK_PROV_CONNECTING);

    HK_CHECK(hk_prov_radios(NULL).ble == false);
    HK_CHECK_EQ_STR(hk_prov_state_name(HK_PROV_ONLINE), "online");
}
