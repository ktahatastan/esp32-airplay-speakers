/**
 * @file hk_main.c
 * @brief Merzarkabul Airplay Speakers application entry point.
 *
 * What this build actually does: come up, report what it is, drive the button
 * and the LED, run the provisioning policy with real radios, hold the output
 * chain's mute line and move it when the gate allows it, and print what
 * every other policy concludes about the state the device is in.
 *
 * What it does not do, and why — each of these waits on a measurement, not on
 * someone finding the time:
 *
 *   F2  the amplifiers have never been run into a dummy load; G1 decides the
 *       settle times hk_audio_hw is currently guessing, sets the profile's
 *       supply-budget stage (supply_budget_sq / supply_window_ms, average
 *       power over both branches) from the adapter's 2.9 A budget with all
 *       four driven on 4 ohm-class loads (S7) -- the peak ceilings are G2's
 *       and stay beneath it -- and measures the adapter's no-load output
 *       before it is connected
 *   F3  the DSP chain -- EQ, subsonic high-pass, LR4 crossover, the
 *       supply-budget stage and a limiter per branch (hk_dsp) -- runs in the
 *       product's output backend, CONFIG_HK_AIRPLAY_OUTPUT_DSP (ADR-0022).
 *       Its numbers wait on measurements: the corners on G0, the budget on
 *       G1, the ceilings and timings on G2. Until a profile carrying them is
 *       in factory_cal the receiver is not started and nothing clocks I2S.
 *       A profile that is there is judged at boot, here, with the same
 *       hk_profile_load() the backend builds its chain from, and a refused
 *       one keeps the DAC muted by name (judge_profile). The bench build
 *       alone compiles in a placeholder profile, and says so when it runs
 *   F7  OTA client compiles but nothing runs it; needs G6
 *
 * Most of the policy modules below are still pure logic with no driver behind
 * them, so report_policies() runs each one and prints its verdict. A policy
 * nobody calls is indistinguishable from one that does not work. hk_audio is
 * the exception as of 2026-09-08: hk_audio_hw drives HK_PIN_DAC_XSMT for
 * real, which means the verdict this file computes is no longer only printed.
 * It is obeyed.
 *
 * Nothing here may grow into driving a real driver without the matching gate.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hk_airplay.h"
#include "hk_audio.h"
#include "hk_audio_hw.h"
#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
#include "hk_tone.h"
#endif
#include "hk_button.h"
#include "hk_eq.h"
#include "hk_identity.h"
#include "hk_led.h"
#include "hk_network.h"
#include "hk_gate.h"
#include "hk_health.h"
#include "hk_ota.h"
#include "hk_ota_client.h"
#include "hk_pins.h"
#include "hk_profile.h"
#include "hk_sched.h"
#include "hk_settings.h"
#include "hk_provision.h"
#include "hk_storage.h"
#include "hk_ui.h"
#include "hk_version.h"

/**
 * Which of the two possible sources of audio this build actually runs.
 *
 * The vendored receiver takes I2S_NUM_0 when it starts and holds it for as long
 * as it lives; so does the bench tone. Two owners of one channel would produce a
 * failure that looks like a DAC fault, which is precisely the fault under
 * investigation, so the exclusion is expressed once, here, and every site that
 * touches the receiver reads it from this macro rather than testing
 * CONFIG_HK_AIRPLAY on its own. A site that forgot would be a site that starts
 * the receiver underneath the tone.
 */
#if CONFIG_HK_AIRPLAY && !CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
#define HK_AIRPLAY_RUNS 1
#else
#define HK_AIRPLAY_RUNS 0
#endif

static const char *TAG = "hk";

/** Read the factory MAC and derive every surface name from it. */
static esp_err_t report_identity(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
        return err;
    }

    hk_identity_t identity;
    int status = hk_identity_from_mac(mac, &identity);
    if (status != HK_IDENTITY_OK) {
        ESP_LOGE(TAG, "identity derivation failed: %d", status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "device id   %s", identity.suffix);
    ESP_LOGI(TAG, "airplay     %s", identity.airplay);
    ESP_LOGI(TAG, "ble         %s", identity.ble);
    ESP_LOGI(TAG, "softap      %s", identity.softap);
    ESP_LOGI(TAG, "mdns        %s.local", identity.mdns);
    return ESP_OK;
}

/** Log the GPIO assignment so a bring-up session can compare it to the sheet. */
static hk_sched_t s_update_schedule;

/*
 * The manifest address is built from the channel setting rather than fixed
 * here; see hk_ota_manifest_url(). A public repository's release assets are
 * fetchable over plain HTTPS with no credential, which is what lets this device
 * carry no token — the OTA plan forbids one, and a device that needs a secret
 * to update is a device whose secret is in its flash.
 */

/** Announced once, so the log says why nothing ever updates. */
static bool s_update_source_reported;

/** The cadence from the OTA plan: a random first delay, then daily. */
static const hk_sched_limits_t s_sched_limits = {
    .interval_ms = HK_SCHED_INTERVAL_MS_DEFAULT,
    .first_delay_ms = HK_SCHED_FIRST_DELAY_MS_DEFAULT,
    .jitter_ms = HK_SCHED_JITTER_MS_DEFAULT,
    .backoff_ms = HK_SCHED_BACKOFF_MS_DEFAULT,
    .backoff_max_ms = HK_SCHED_BACKOFF_MAX_MS_DEFAULT,
};

/**
 * Milliseconds since boot.
 *
 * The provisioning policy is written against a real clock, and until now every
 * call site passed a literal 0. That made the bounded window — the ten minutes
 * after which a setup session opened on an already-configured speaker closes
 * itself — unreachable: no time ever passed, so it could never expire.
 */
static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * What the judge said about the stored profile, for the boot report.
 *
 * "absent" when there is no blob; otherwise the one-word verdict of
 * hk_profile_load() ("ok", "ceiling", "schema", ...), or, when the bytes could
 * not even be read, the store's own reason. Set once by judge_profile() and
 * never recomputed: the report prints what the gate was decided on.
 */
static const char *s_profile_verdict = "unjudged";

/**
 * Judge the stored profile, and tell storage what was decided.
 *
 * One judge (ADR-0022). The output backend builds its chain with
 * hk_profile_load(); this calls the same function on the same bytes at the
 * same output rate and supply voltage, so the boot gate and the backend cannot
 * disagree about a blob. Until this ran, a present profile permitted audio on
 * presence alone -- which meant a blob the backend would refuse still released
 * the DAC mute into a chain writing digital zero. The verdict goes into
 * hk_storage, which is where every caller of hk_storage_audio_permitted()
 * already reads the gate: the mute sequence below and the receiver's own
 * refusal to start.
 *
 * Verdict only. The chain the backend will run is built by the backend, on
 * the playback task, from the same call; nothing built here would have
 * anywhere to go, and two chains from one blob is one more than one owner.
 *
 * CONFIG_OUTPUT_SAMPLE_RATE_HZ exists only inside `if HK_AIRPLAY`, and this
 * file supports a build without the receiver, so that build judges the blob
 * structurally: there is no output rate to build a chain at and nothing that
 * would clock I2S if there were.
 */
static void judge_profile(void)
{
    if (!hk_storage_profile_present()) {
        s_profile_verdict = "absent";
        return;
    }

    /* A store that hk_storage will not read from -- schema fail-safe -- has
     * already shut the gate on its own; the report should say that rather
     * than a read error dressed as a verdict. */
    const hk_schema_action_t store = hk_storage_factory_action();
    if (!hk_schema_audio_permitted(store)) {
        s_profile_verdict = hk_schema_action_name(store);
        hk_storage_profile_judged(false);
        ESP_LOGE(TAG, "profile     present, but the calibration store is %s: not read",
                 s_profile_verdict);
        return;
    }

    /* Into a buffer of exactly one profile, so the read itself fails on a blob
     * of another length rather than trusting it: hk_profile_from_blob() would
     * refuse a wrong length by name ("schema"), and a blob too long for this
     * buffer is refused before it, by the store, as an invalid length. */
    uint8_t raw[sizeof(hk_profile_t)];
    size_t  length = sizeof(raw);
    const esp_err_t err = hk_storage_factory_get_blob(HK_STORAGE_PROFILE_KEY, raw, &length);
    if (err != ESP_OK) {
        s_profile_verdict = esp_err_to_name(err);
        hk_storage_profile_judged(false);
        ESP_LOGE(TAG, "profile     present but unreadable: %s", s_profile_verdict);
        return;
    }

    hk_profile_t profile;
#if CONFIG_HK_AIRPLAY
    const hk_profile_verdict_t verdict =
        hk_profile_load(raw, length, (float)CONFIG_OUTPUT_SAMPLE_RATE_HZ,
                        (float)CONFIG_HK_SUPPLY_MV, &profile, NULL);
#else
    const hk_profile_verdict_t verdict = hk_profile_from_blob(raw, length, &profile);
#endif
    s_profile_verdict = hk_profile_verdict_name(verdict);
    hk_storage_profile_judged(verdict == HK_PROFILE_OK);

    if (verdict == HK_PROFILE_OK) {
#if CONFIG_HK_AIRPLAY
        ESP_LOGI(TAG, "profile     present, judged %s at %d Hz for a %d mV supply",
                 s_profile_verdict, (int)CONFIG_OUTPUT_SAMPLE_RATE_HZ,
                 (int)CONFIG_HK_SUPPLY_MV);
#else
        ESP_LOGI(TAG, "profile     present, judged %s (structurally: no output rate "
                      "in this build)", s_profile_verdict);
#endif
        /* A profile whose source name begins with "provisional" is the
         * record's bench listening profile (docs/04-acoustics/
         * measurement-and-dsp-plan.md): written by the owner for the staged
         * pair, its corners and ceilings placeholders. The gate treats it
         * like any valid profile -- that is what it is for -- so the boot
         * report has to be the thing that says it is not a calibration.
         * The phrase deliberately differs from the bench build's own
         * warning, which CI asserts the product image does not contain. */
        if (strncmp(profile.source, "provisional", 11) == 0) {
            ESP_LOGW(TAG, "profile     source '%s' is PROVISIONAL: a bench listening profile, "
                          "not a calibration. Staged pair only (one amplifier, one woofer, "
                          "one tweeter), low level, supply at or below 15 V; nothing in it "
                          "was measured except the two DC resistances.",
                     profile.source);
        }
    } else {
        ESP_LOGE(TAG, "profile     present and REFUSED: %s", s_profile_verdict);
    }
}

/**
 * Whether sound is allowed, as the whole device sees it.
 *
 * One gate: storage asks whether a driver-protection profile exists and was
 * judged valid at boot (judge_profile), and the bench exception it carries is
 * applied inside that answer. It is wrapped here rather than called directly
 * from each site because hk_audio_hw is a hardware layer that drives two pins
 * and should hold no policy, and because the place the verdict is computed
 * should be the place it is printed.
 */
static bool audio_permitted_now(void)
{
    return hk_storage_audio_permitted();
}

/** One row of a settings table, resolved the way its reader will resolve it. */
static void report_setting(const hk_setting_def_t *def)
{
    uint32_t stored = 0;
    const bool present = hk_storage_user_read_u32(def->key, &stored);
    hk_setting_origin_t origin;
    const uint32_t value = hk_settings_resolve(def, stored, present, &origin);
    const char *source = (origin == HK_SETTING_STORED) ? "stored"
                       : (origin == HK_SETTING_OUT_OF_RANGE) ? "OUT OF RANGE -> default"
                       : "default";
    ESP_LOGI(TAG, "  %-12s %-6" PRIu32 " %s", def->key, value, source);
}

/**
 * Run each policy against what the device actually knows, and print the answer.
 *
 * This is not decoration. Every module below is pure logic with no driver
 * behind it yet, and a policy nobody calls is indistinguishable from one that
 * does not work. Reporting what each one concludes at boot makes them reachable
 * and turns the boot log into evidence: today it should say that audio is not
 * permitted, that the image cannot be confirmed, and that no setting came from
 * storage, because none of those things are true yet.
 */
static void report_policies(void)
{
    ESP_LOGI(TAG, "settings (defaults until a value is stored)");
    for (size_t i = 0; i < hk_settings_count(); i++) {
        report_setting(&hk_settings_table[i]);
    }
    /* The equaliser's rows are settings of the same kind, read from the same
     * store by the same rules, and they are listed here rather than in the
     * table above because they cannot be spliced into it: hk_audio requires
     * hk_settings for the row type, so hk_settings cannot require hk_audio
     * back (hk_eq.h). This file links both, so this is where the two tables
     * meet. Every row at its default is a flat equaliser: the chain runs
     * bit-exact passthrough, and the row still deserves a line, because a
     * stored value that made the sound change overnight would be found here. */
    ESP_LOGI(TAG, "eq settings (tonal; flat until a value is stored, read at playback start)");
    for (size_t i = 0; i < hk_eq_settings_count(); i++) {
        report_setting(&hk_eq_settings_table[i]);
    }

    /* The gate and the verdict, side by side. The gate is what the profile
     * store holds: nothing, or a blob and the judge's one-word answer on it.
     * The verdict is what hk_audio_hw is actually being told, and the two are
     * not always the same: a bench exception can lift the absence refusal, and
     * a report that showed only the raw gate would say "absent" while the
     * amplifier came up underneath it. "present:ok" is the only gate state
     * that permits audio on its own. */
    ESP_LOGI(TAG, "audio       profile %s%s · verdict %s",
             hk_storage_profile_present() ? "present:" : "",
             s_profile_verdict,
             audio_permitted_now() ? "PERMITTED" : "muted");
#if CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE
    if (!hk_storage_profile_present()) {
        ESP_LOGW(TAG, "audio       BENCH EXCEPTION: no driver-protection profile exists "
                      "and the audio path is permitted anyway "
                      "(CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE). Only safe with nothing "
                      "connected to the output.");
    }
#endif
    if (audio_permitted_now()) {
        /* Loud, and only when the verdict has actually become an unmuted DAC
         * feeding amplifiers that have no mute of their own. This is the state
         * the bench exception exists to make visible rather than to make
         * convenient. What sits in front of the amplifiers depends on which
         * output backend this build selected, and the line says which, because
         * "protected" printed over the vendored stage would be the wrong thing
         * to leave next to a released XSMT. */
#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
        ESP_LOGW(TAG, "audio       THE DAC WILL BE UNMUTED INTO LIVE AMPLIFIERS when the "
                      "tone starts. The bench tone is written straight to I2S, past "
                      "hk_dsp: no crossover, no protective high-pass and no limiter in "
                      "front of them. Check what is on the speaker terminals.");
#elif CONFIG_HK_AIRPLAY_OUTPUT_DSP && CONFIG_HK_BENCH_PROVISIONAL_PROFILE
        /* The bench build is the one place a placeholder profile is compiled
         * in, so it is the one place this sentence may say so. CI checks the
         * product image for the words "placeholders until" and refuses it if
         * they are there: on a product board they would describe a profile
         * the board does not have. */
        ESP_LOGW(TAG, "audio       THE DAC WILL BE UNMUTED INTO LIVE AMPLIFIERS when a "
                      "stream arrives. The DSP chain (EQ, subsonic high-pass, LR4 "
                      "crossover, supply-budget stage, limiter per branch) is in the "
                      "path -- on the stored profile if this boot judged one valid, "
                      "otherwise on the compiled-in bench profile, whose corners, "
                      "ceilings and budget are placeholders until G0/G1/G2 and which "
                      "announces itself when the backend starts. Check what is on the "
                      "speaker terminals.");
#elif CONFIG_HK_AIRPLAY_OUTPUT_DSP
        ESP_LOGW(TAG, "audio       THE DAC WILL BE UNMUTED INTO LIVE AMPLIFIERS when a "
                      "stream arrives. The DSP chain (EQ, subsonic high-pass, LR4 "
                      "crossover, supply-budget stage, limiter per branch) is in the "
                      "path, on the stored profile judged above. Check what is on the "
                      "speaker terminals.");
#elif CONFIG_HK_AIRPLAY
        ESP_LOGW(TAG, "audio       THE DAC WILL BE UNMUTED INTO LIVE AMPLIFIERS when a "
                      "stream arrives. This build selects one of upstream's own output "
                      "stages, not the product's DSP backend (ADR-0022): no crossover, "
                      "no protective high-pass and no limiter in front of them. Check "
                      "what is on the speaker terminals.");
#else
        /* Reachable: the receiver off and the bench exception on is a build
         * nobody ships but the tree allows, and until this branch existed it
         * printed the sentence above -- an output stage selected in a build
         * that has no output stage. The sequence releases the DAC mute on
         * permitted AND a live stream, and nothing in this build reports one,
         * so the verdict is a permission with nothing to act on it. Still a
         * warning: the gate is open, and the first thing that pushes a
         * stream would find it so. */
        ESP_LOGW(TAG, "audio       PERMITTED, but no output backend is built into this "
                      "build: nothing writes to I2S and nothing reports a live stream, "
                      "so the sequence has nothing to release the DAC mute on. The gate "
                      "is open all the same. Check what is on the speaker terminals.");
#endif
    }

    /* The output chain starts muted: the DAC's XSMT is held down by an
     * external pull-down, not by this firmware (ADR-0011), and the amplifiers
     * behind it have no mute of their own.
     *
     * "Starts", not "stays", and that used to be true of the I2S clocks alone.
     * It is now true of both lines. Where the receiver runs it clocks I2S as
     * soon as it comes up, seconds after this line is printed; and since
     * hk_audio_hw exists, the DAC's mute line is no longer permanently
     * asserted either -- it follows the sequence, and the sequence follows
     * the verdict printed above. The suffix says which of those applies to
     * this build, because a line that read as a promise about the rest of the
     * boot would be the wrong thing to leave in a log next to an amplifier.
     *
     * Whether the receiver runs at all is the gate's answer again: on the
     * bring-up devkit hk_airplay starts it regardless, because there is no
     * DAC on those pins; on any other board hk_airplay_start() refuses on the
     * same hk_storage_audio_permitted() this file prints, so a muted verdict
     * means the receiver is never started and nothing clocks I2S.
     *
     * This chain is a throwaway used to name the resting state. The one that
     * drives the pins belongs to hk_audio_hw and is started further down. */
    hk_audio_t chain;
    hk_audio_init(&chain, 0);
    const hk_audio_outputs_t lines = hk_audio_outputs(chain.state);
    ESP_LOGI(TAG, "output      %s (i2s=%d dac=%d)%s",
             hk_audio_state_name(chain.state),
             lines.i2s_running, lines.dac_unmuted,
             audio_permitted_now()
                 ? ", until a stream arrives and the sequence releases the DAC mute"
#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
                 : ", and the DAC stays muted; only I2S is clocked, by the bench tone"
#elif CONFIG_HK_AIRPLAY && CONFIG_HK_BOARD_DEVKIT_N8R2
                 : ", and the DAC stays muted; only I2S is clocked, by the receiver"
#elif CONFIG_HK_AIRPLAY
                 : ", and the DAC stays muted; the receiver is not started without a "
                   "permitted profile, so nothing clocks I2S"
#else
                 : ""
#endif
             );

    /* What the first-boot check would decide right now. Nothing reports yet,
     * so it must be waiting rather than confirming. */
    const hk_health_limits_t health_limits = {
        .settle_ms = HK_HEALTH_SETTLE_MS_DEFAULT,
        .deadline_ms = HK_HEALTH_DEADLINE_MS_DEFAULT,
    };
    const hk_health_inputs_t health_now = {
        .storage = HK_HEALTH_UNKNOWN,
        .network = HK_HEALTH_UNKNOWN,
        .audio = HK_HEALTH_SKIP,
        .uptime_ms = 0,
        .critical_fault = false,
    };
    hk_health_reason_t why;
    const hk_health_verdict_t verdict =
        hk_health_evaluate(&health_now, &health_limits, &why);
    ESP_LOGI(TAG, "first boot  %s (%s)",
             hk_health_verdict_name(verdict), hk_health_reason_name(why));

    /* When this speaker would look for an update, if there were one to find.
     * The first check waits a random while after boot rather than landing on
     * the instant Wi-Fi comes up, so a power cut does not put the check at a
     * fixed instant after every restart (see hk_sched.h). */
    hk_sched_init(&s_update_schedule, now_ms(), esp_random(), &s_sched_limits);
    ESP_LOGI(TAG, "update      first check in %" PRIu32 " s (no release source configured yet)",
             hk_sched_remaining(&s_update_schedule, now_ms()) / 1000u);
}

/**
 * One turn of the update loop.
 *
 * Kept here rather than inside hk_ota because it is application wiring: which
 * product this is, which channel it follows, what state it is in. hk_ota stays
 * a component that judges a manifest and writes a slot.
 */
/** Read a user setting through its definition, so the range is applied. */
static uint32_t setting_u32(const char *key)
{
    const hk_setting_def_t *def = hk_settings_find(key);
    uint32_t stored = 0;
    const bool present = def != NULL && hk_storage_user_read_u32(key, &stored);
    return hk_settings_resolve(def, stored, present, NULL);
}

/**
 * Remember how the last pending image was judged.
 *
 * Called from the health monitor immediately before it acts, because the
 * rollback path reboots and never comes back. A device that rolls back, fetches
 * the same release again and rolls back again is spending its flash on one
 * mistake nightly; the counter is what stops that.
 */
static void persist_health_verdict(bool confirmed)
{
    if (confirmed) {
        (void)hk_storage_user_set_u32("rollbacks", 0);
        return;
    }
    const uint32_t previous = setting_u32("rollbacks");
    const uint32_t next = (previous < 255u) ? previous + 1u : previous;
    (void)hk_storage_user_set_u32("rollbacks", next);
    ESP_LOGE(TAG, "consecutive rollbacks: %" PRIu32, next);
}

static void run_update_check(void)
{
    const uint32_t rollbacks = setting_u32("rollbacks");
    if (!hk_ota_updates_allowed(rollbacks)) {
        if (!s_update_source_reported) {
            s_update_source_reported = true;
            ESP_LOGE(TAG, "updates stopped after %" PRIu32 " consecutive rollbacks. "
                          "This needs a person: a release that fixes it, over USB, "
                          "or the counter cleared deliberately.", rollbacks);
        }
        return;
    }

    char manifest_url[HK_OTA_URL_MAX];
    if (!hk_ota_manifest_url(manifest_url, sizeof(manifest_url),
                             setting_u32("channel"))) {
        if (!s_update_source_reported) {
            s_update_source_reported = true;
            ESP_LOGE(TAG, "could not build a manifest address; updates are off");
        }
        return;
    }

    if (!hk_sched_due(&s_update_schedule, now_ms())) {
        return;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *inactive = esp_ota_get_next_update_partition(NULL);

    hk_ota_request_t request = {
        .manifest_url = manifest_url,
        .device = {
            .product = app->project_name,
            .target = "esp32s3",
            .hw_revision = HK_HW_REVISION,
            /* From storage, so the device can be moved to the canary channel
             * to try a release before it is promoted, without building it a
             * different image — a canary running different firmware is not
             * testing the release that will be promoted. */
            .channel = hk_ota_channel_name(setting_u32("channel")),
            .running_version = app->version,
            .running_secure_version = app->secure_version,
            .slot_size = inactive != NULL ? inactive->size : 0u,
        },
        .gate_inputs = {
            .audio_active = false,
            .wifi_connected = true,
            .update_in_progress = false,
        },
    };

    hk_ui_set_ota(true);
    const esp_err_t err = hk_ota_client_run(&request);
    hk_ui_set_ota(false);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "update written; it boots on the next restart");
        hk_sched_success(&s_update_schedule, now_ms(), esp_random(), &s_sched_limits);
    } else if (err == ESP_ERR_NOT_FOUND) {
        /* Nothing newer is a successful check, not a failure. Backing off here
         * would slow a healthy speaker down for doing the right thing. */
        hk_sched_success(&s_update_schedule, now_ms(), esp_random(), &s_sched_limits);
    } else {
        ESP_LOGW(TAG, "update check failed: %s", esp_err_to_name(err));
        hk_sched_failure(&s_update_schedule, now_ms(), esp_random(), &s_sched_limits);
    }
}

static void report_pins(void)
{
    ESP_LOGI(TAG, "gpio assignment (candidate, see circuit-and-wiring-plan section 3.1)");
    for (int i = 0; i < hk_pin_table_size(); i++) {
        ESP_LOGI(TAG, "  %-10s GPIO%d", hk_pin_table[i].role, hk_pin_table[i].gpio);
    }
}

/**
 * Report the running slot and firmware version.
 *
 * The image is NOT marked valid here. That happens only after the first-boot
 * health check defined in docs/03-firmware/ota-and-release-plan.md, which is
 * F7 work; marking it valid now would defeat the rollback that ADR-0008 relies
 * on. CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE stays off until then, so this build
 * boots normally and the report is informational.
 */
static void report_build(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(TAG, "firmware    %s", app->version);
    ESP_LOGI(TAG, "idf         %s", app->idf_ver);
    ESP_LOGI(TAG, "built       %s %s", app->date, app->time);
    if (running != NULL) {
        ESP_LOGI(TAG, "slot        %s at 0x%08" PRIx32 ", %" PRIu32 " bytes",
                 running->label, running->address, running->size);
    }

    hk_version_t parsed;
    if (hk_version_parse(app->version, &parsed) != HK_VERSION_OK) {
        /* PROJECT_VER comes from firmware/version.txt. If it is not strict
         * SemVer the OTA client can never compare it, so the release pipeline
         * would publish an image no device would accept. */
        ESP_LOGE(TAG, "PROJECT_VER '%s' is not strict SemVer; OTA comparison would fail",
                 app->version);
    }
}

/**
 * Report the hardware actually present.
 *
 * Flash size is read with esp_flash_get_physical_size() rather than
 * esp_flash_get_size(): the latter returns the size recorded in the binary
 * image header, which is whatever CONFIG_ESPTOOLPY_FLASHSIZE was set to. It
 * would echo the build configuration back and never disagree with it.
 *
 * There is deliberately no "flash is too small" branch here. ESP-IDF already
 * refuses to start on a part smaller than the image header claims:
 * esp_flash_init_default_chip() returns ESP_ERR_FLASH_SIZE_NOT_MATCH and
 * startup asserts on it, so such a board panics before app_main runs. A check
 * here could never fire, and a check that cannot fire is worse than none:
 * it reads like assurance.
 *
 * PSRAM is reported because it is the half of "N16R8" that flash size alone
 * cannot distinguish; an N16 board without PSRAM boots fine and would otherwise
 * look correct in this report.
 *
 * PSRAM size is also the one board fact worth checking rather than only printing,
 * and unlike the flash case above this check can fire. The size is detected at
 * runtime while the board variant is a build-time decision, so the two can
 * disagree: a devkit image flashed onto a product board, or the reverse. Both
 * boot. Both then run with a partition table built for the other part. Saying so
 * here is the only place that disagreement becomes visible.
 */
#if CONFIG_HK_BOARD_DEVKIT_N8R2
#define HK_EXPECTED_PSRAM_MB 2u
#else
#define HK_EXPECTED_PSRAM_MB 8u
#endif

static void report_hardware(void)
{
    /* Which board this image was built for, not what is under it. The OTA
     * manifest is matched against exactly this string, so a device that is
     * refusing every release can be diagnosed from its own boot log. */
    ESP_LOGI(TAG, "board       %s", HK_HW_REVISION);

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "chip        %d core(s), revision %d", chip.cores, chip.revision);

    uint32_t flash_size = 0;
    esp_err_t err = esp_flash_get_physical_size(NULL, &flash_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "flash       %" PRIu32 " MB detected", flash_size / (1024U * 1024U));
    } else {
        ESP_LOGW(TAG, "flash       size not detected: %s", esp_err_to_name(err));
    }

    const size_t psram = esp_psram_get_size();
    const unsigned psram_mb = (unsigned)(psram / (1024U * 1024U));
    if (psram == 0) {
        ESP_LOGE(TAG, "psram       none found; %s expects %u MB",
                 HK_HW_REVISION, HK_EXPECTED_PSRAM_MB);
    } else if (psram_mb != HK_EXPECTED_PSRAM_MB) {
        ESP_LOGE(TAG, "psram       %u MB, but this image is built for %s with %u MB. "
                      "This is the wrong image for this board: its partition table "
                      "describes a different part.",
                 psram_mb, HK_HW_REVISION, HK_EXPECTED_PSRAM_MB);
    } else {
        ESP_LOGI(TAG, "psram       %u MB", psram_mb);
    }

    /* The memory budget, printed rather than assumed.
     *
     * The AirPlay receiver of ADR-0007 sizes its jitter buffer from a compile
     * time constant, so whether it fits is decided by what is left AFTER the
     * radios and TLS are resident -- not by the size of the part. Printing both
     * pools at the same point in every boot is what makes that a number instead
     * of an argument, and what makes a regression visible when some later
     * component starts allocating at init. */
    ESP_LOGI(TAG, "free        %u B internal (largest block %u B), %u B psram",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/** Provisioning policy for this boot. Nothing drives the radios yet. */
/*
 * The provisioning state is touched from two tasks: on_button() runs in the
 * hk_ui task, and the supervisory loop below runs in app_main. Both cores are
 * enabled, so those are genuinely concurrent, and hk_prov_t is several fields
 * that have to change together — a button press landing between the loop's
 * read of `state` and its read of `opened_ms` would act on half of each.
 *
 * The lock lives here rather than in hk_provision, which is deliberately pure
 * C with no RTOS dependency so it can be host-tested. A module that took a
 * FreeRTOS mutex could not be run on a laptop.
 */
static hk_prov_t    s_provisioning;
static portMUX_TYPE s_prov_lock = portMUX_INITIALIZER_UNLOCKED;
static bool         s_prov_ready;

/*
 * Work the button asks for, carried out on the main task instead of in the UI
 * task that observed the press.
 *
 * The UI task exists to read one GPIO and drive three PWM channels, and it is
 * sized for that. Opening provisioning is the opposite kind of work: it brings
 * up NimBLE, protocomm and its key exchange, none of which fit in a stack
 * measured for debouncing. Calling it from the button callback put that whole
 * stack on the UI task and overflowed it -- a press while the speaker was
 * playing took the device down. The other two actions are lighter but the same
 * shape: NVS writes reached from a task that should only be reading a pin.
 *
 * So the callback records intent and returns. Every side effect happens on the
 * main task, which already owns the provisioning window and closes it. That
 * also removes an asymmetry that was easy to miss: the window was opened from
 * one task and closed from another.
 *
 * Guarded by s_prov_lock, and set only when the policy accepted the event, so a
 * press arriving before start_network() finishes is dropped in one piece rather
 * than half-applied.
 */
#define HK_ACTION_OPEN_PROVISIONING   (1u << 0)
#define HK_ACTION_FORGET_CREDENTIALS  (1u << 1)
#define HK_ACTION_RESET_USER_SETTINGS (1u << 2)
#define HK_ACTION_START_AIRPLAY       (1u << 3)

static uint32_t     s_pending_actions;
static TaskHandle_t s_main_task;

/** Apply one provisioning event under the lock. */
static void prov_event(hk_prov_event_t event)
{
    const uint32_t at = now_ms();
    portENTER_CRITICAL(&s_prov_lock);
    if (s_prov_ready) {
        hk_prov_handle(&s_provisioning, event, at);
    }
    portEXIT_CRITICAL(&s_prov_lock);
}

/**
 * Apply a button event and queue the work it implies, atomically.
 *
 * Both halves happen under one lock so the policy state and the pending work
 * can never disagree about whether a press was accepted.
 */
static void button_request(hk_prov_event_t event, uint32_t actions)
{
    const uint32_t at = now_ms();
    bool accepted = false;
    portENTER_CRITICAL(&s_prov_lock);
    if (s_prov_ready) {
        hk_prov_handle(&s_provisioning, event, at);
        /* Queue only the work the policy actually decided on.
         *
         * A short press on a speaker that is already online now ARMS a
         * confirmation rather than opening a window, and the difference has to
         * survive this far: opening the radios calls esp_wifi_disconnect(), so
         * queueing the action anyway would drop the network on exactly the
         * press the policy just declined to act on. Asking hk_prov_radios()
         * what it wants is the check -- it is the same function the supervisory
         * loop already trusts, so there is no second opinion to keep in step. */
        const hk_prov_radios_t want = hk_prov_radios(&s_provisioning);
        if (!want.ble && !want.softap) {
            actions &= ~HK_ACTION_OPEN_PROVISIONING;
        }
        s_pending_actions |= actions;
        accepted = true;
    }
    portEXIT_CRITICAL(&s_prov_lock);

    /* Outside the critical section: notifying can reschedule. The loop also
     * wakes on its own once a second, so a lost notification costs latency,
     * not the action. */
    if (accepted && s_main_task != NULL) {
        xTaskNotifyGive(s_main_task);
    }
}

/**
 * Queue work without a policy event behind it.
 *
 * The network status callback runs in the event task, which is no better a
 * place to start an AirPlay receiver than the UI task was to start BLE.
 */
static void queue_action(uint32_t actions)
{
    portENTER_CRITICAL(&s_prov_lock);
    s_pending_actions |= actions;
    portEXIT_CRITICAL(&s_prov_lock);
    if (s_main_task != NULL) {
        xTaskNotifyGive(s_main_task);
    }
}

/** Take the queued work, leaving the queue empty. */
static uint32_t take_pending_actions(void)
{
    portENTER_CRITICAL(&s_prov_lock);
    const uint32_t actions = s_pending_actions;
    s_pending_actions = 0u;
    portEXIT_CRITICAL(&s_prov_lock);
    return actions;
}

/** A consistent copy, so callers never read a half-updated struct. */
static hk_prov_t prov_snapshot(void)
{
    hk_prov_t copy;
    portENTER_CRITICAL(&s_prov_lock);
    copy = s_provisioning;
    portEXIT_CRITICAL(&s_prov_lock);
    return copy;
}

/**
 * Act on a committed button gesture.
 *
 * Runs in the UI task, so it stays short, and touches the provisioning state
 * only through prov_event() because app_main's loop touches it too.
 *
 * The destructive branches really do erase: a network reset clears the stored
 * Wi-Fi credentials, and a factory reset restores user settings as well. What
 * neither can reach is the calibration partition — hk_storage opens it
 * read-only and names it in no erase call, which is the structural half of
 * PRD-008 that tools/check_storage_isolation.py checks in CI.
 */
static void on_button(hk_button_event_t event, void *context)
{
    (void)context;
    switch (event) {
    case HK_BUTTON_EVENT_SHORT_PRESS: {
        button_request(HK_PROV_EV_BUTTON_SHORT, HK_ACTION_OPEN_PROVISIONING);
        /* Say which of the two things happened. The old line claimed to be
         * opening provisioning on every press, which became a lie the moment
         * the first press on an online speaker started only arming. */
        const hk_prov_t after = prov_snapshot();
        if (hk_prov_confirm_pending(&after, now_ms())) {
            ESP_LOGI(TAG, "button: armed -- press again within %u ms to open setup "
                          "(this speaker is online, and opening it drops the network)",
                     (unsigned)HK_PROV_CONFIRM_MS);
        } else {
            ESP_LOGI(TAG, "button: opening provisioning -> %s",
                     hk_prov_state_name(after.state));
        }
        break;
    }
    case HK_BUTTON_EVENT_NETWORK_RESET:
        ESP_LOGW(TAG, "button: forgetting Wi-Fi credentials");
        button_request(HK_PROV_EV_NETWORK_RESET, HK_ACTION_FORGET_CREDENTIALS);
        break;
    case HK_BUTTON_EVENT_FACTORY_RESET:
        ESP_LOGW(TAG, "button: restoring user settings to defaults");
        button_request(HK_PROV_EV_FACTORY_RESET,
                       HK_ACTION_RESET_USER_SETTINGS | HK_ACTION_FORGET_CREDENTIALS);
        break;
    case HK_BUTTON_EVENT_NONE:
    default:
        break;
    }
}

/** Mirror the network layer's state onto the status LED. */
#if HK_AIRPLAY_RUNS
/**
 * Playback started or stopped.
 *
 * The LED has one owner, and this is how a fact from another subsystem reaches
 * it: hk_ui arbitrates, so playback can never outrank a fault. The precedence
 * lives in hk_led, not here.
 *
 * It is also the stream_live signal the output sequence runs on. The two
 * consumers are told in the order they matter in: the mute sequence is what
 * decides whether a tweeter sees anything, the LED is what decides whether a
 * person sees anything. Both calls do nothing but store a value, which is all
 * the RTSP task should be asked to pay for.
 *
 * Note what "playing" does NOT mean here: RTSP_EVENT_CLIENT_CONNECTED is
 * excluded upstream in hk_airplay.c, so a phone that has selected this speaker
 * without starting a track leaves the amplifier down. That is the right way
 * round -- a session with no audio in it is not a reason to energise anything.
 */
static void on_airplay_state(bool playing, void *context)
{
    (void)context;
    hk_audio_hw_set_stream_live(playing);
    hk_ui_set_playing(playing);
}
#endif /* HK_AIRPLAY_RUNS */

#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
/**
 * The bench signal has ended.
 *
 * The same fact, in the same shape, as on_airplay_state(false): whatever was
 * feeding I2S has stopped, so the claim of a live stream has to be withdrawn
 * and hk_audio's sequence puts the DAC and the amplifier back down.
 *
 * The fixed tone never ends, so in that build this never runs. The sweep does,
 * after its last step -- and it also runs if the sweep ABANDONS itself on an
 * I2S error, which is the case worth stating: the two outcomes differ in what
 * the operator learned, not in what the output chain should be doing
 * afterwards, and a failed run that left the amplifier released would be the
 * worse of the two.
 *
 * Called on the generator task, so it does what the receiver's callback does
 * and no more: stores a value.
 */
static void on_tone_done(void *context)
{
    (void)context;
    hk_audio_hw_set_stream_live(false);
    ESP_LOGW(TAG, "the bench signal has ended; the DAC mute is going back down. "
                  "Nothing here is a measurement until you write it into "
                  "docs/02-hardware/driver-measurements.md.");
}
#endif /* CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY */

static void on_network_status(const hk_net_status_t *network, void *context)
{
    (void)context;
    hk_ui_set_network(network->provisioning, network->connecting, network->connected);
    hk_ui_set_fault(HK_UI_FAULT_NETWORK, network->error);

    /* For the first-boot check. Joined, or provisioning open on purpose, both
     * count as working: a speaker whose owner changed their Wi-Fi password has
     * a network problem, not a firmware problem, and rolling back would not
     * help because the previous image cannot connect either. Only the stack
     * itself failing is a failure. */
    if (network->error) {
        hk_health_report(HK_HEALTH_CRITERION_NETWORK, HK_HEALTH_FAIL);
    } else if (network->connected || network->provisioning) {
        hk_health_report(HK_HEALTH_CRITERION_NETWORK, HK_HEALTH_PASS);
    }

    /* The memory budget again, at the moment the AirPlay receiver would start.
     *
     * The boot-time figures in report_hardware() are taken before the Wi-Fi
     * driver allocates anything, so they overstate what a receiver would find.
     * This is the number that decides whether the ADR-0007 jitter buffer fits,
     * so it is logged where that decision is actually made -- once per join,
     * not per status change, or a flapping link would fill the log. */
    static bool first_join;
    if (network->connected && !first_join) {
        first_join = true;
        /* The receiver needs an address, so this is the earliest it can start,
         * and it starts on the main task rather than here. */
        queue_action(HK_ACTION_START_AIRPLAY);
        ESP_LOGI(TAG, "free (joined) %u B internal (largest block %u B), %u B psram, "
                      "ui task %u B stack unused",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned)hk_ui_stack_headroom());
    }
}

/**
 * Bring up the network.
 *
 * A failure here is reported and survived rather than fatal: a device that
 * reboots forever cannot tell anyone why. It stays up, lights the error state,
 * and says what went wrong. (Until ADR-0023 the likeliest failure on a fresh
 * board was a calibration store with no provisioning credentials in it; setup
 * needs none now, so a blank store is not a failure any more.)
 */
static void start_network(void)
{
    /* The UI task is already running by now, so a press could arrive during
     * this call. Publishing readiness under the same lock means such a press
     * is dropped rather than applied to a half-built state. */
    const bool provisioned = hk_network_is_provisioned();
    const bool recovery = hk_ui_recovery_requested();
    const uint32_t at = now_ms();
    portENTER_CRITICAL(&s_prov_lock);
    hk_prov_init(&s_provisioning, provisioned, recovery, at);
    s_prov_ready = true;
    portEXIT_CRITICAL(&s_prov_lock);

    const hk_prov_t started = prov_snapshot();
    ESP_LOGI(TAG, "provisioning policy: %s, bounded=%d",
             hk_prov_state_name(started.state), started.bounded);

    /* The transport follows the situation rather than a choice made here:
     * SoftAP with nothing stored, BLE from a button press on a configured
     * device. ADR-0005 option C; the reasoning is in hk_network.h. */
    esp_err_t err = hk_network_start(on_network_status, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "network did not start: %s", esp_err_to_name(err));
        hk_ui_set_fault(HK_UI_FAULT_NETWORK, true);
        hk_health_report(HK_HEALTH_CRITERION_NETWORK, HK_HEALTH_FAIL);
    }
    /* Deliberately not waiting for the first status event to decide. If the
     * stack came up but no event ever arrived, the criterion would sit unknown
     * until the deadline and roll a working image back. */

    ESP_LOGI(TAG, "button       short %u-%u ms, network reset %u ms, factory reset %u ms",
             (unsigned)HK_BUTTON_SHORT_MIN_MS, (unsigned)HK_BUTTON_SHORT_MAX_MS,
             (unsigned)HK_BUTTON_NETWORK_MS, (unsigned)HK_BUTTON_FACTORY_MS);
}

void app_main(void)
{
    /* Opens both stores and works out what state each is in. Never fatal: an
     * unusable store is reported, because a speaker that will not boot cannot
     * tell anyone why. */
    /* Before anything else: is this image awaiting judgement at all? On a
     * USB-flashed build the answer is no and the monitor stays silent. */
    hk_health_monitor_set_persist(persist_health_verdict);
    (void)hk_health_monitor_begin();

    const esp_err_t storage_err = hk_storage_init();
    hk_health_report(HK_HEALTH_CRITERION_STORAGE,
                     storage_err == ESP_OK ? HK_HEALTH_PASS : HK_HEALTH_FAIL);
    ESP_ERROR_CHECK(storage_err);

    /* Storage knows whether a profile is there; it does not know whether it
     * is any good, and the gate needs both. Judged here, before anything
     * reads the gate, so the first answer hk_audio_hw and the receiver get
     * is already the real one. */
    judge_profile();

    ESP_LOGI(TAG, "%s", HK_PRODUCT_FAMILY);
    report_build();
    report_hardware();
    ESP_ERROR_CHECK(report_identity());
    report_pins();
    ESP_LOGI(TAG, "storage     user=%s calibration=%s",
             hk_schema_action_name(hk_storage_user_action()),
             hk_schema_action_name(hk_storage_factory_action()));
    report_policies();
    if (!audio_permitted_now()) {
        if (hk_storage_profile_present()) {
            /* The third refusal, and the one that needs its reason printed:
             * the board LOOKS calibrated. Nothing here can mend the blob --
             * factory_cal is opened read-only -- so the answer is the bench
             * that wrote it, and the verdict is what it needs to hear. "Or
             * could not be read", because the word in the brackets is not
             * always the judge's: when the store is fail-safe it is the
             * store's action name, and when the blob would not fit one
             * profile it is the read's esp_err name. judge_profile() said
             * which a few lines up; this line has to be true on its own. */
            ESP_LOGE(TAG, "audio is NOT permitted: the stored profile was refused or "
                          "could not be read (%s); audio stays muted and the receiver "
                          "is not started. The blob in factory_cal has to be rewritten "
                          "by the bench that produced it; this firmware cannot.",
                     s_profile_verdict);
        } else {
            ESP_LOGE(TAG, "audio is NOT permitted: this device has no trustworthy driver "
                          "protection profile. No default profile is invented (G0/G2).");
        }
    } else if (!hk_storage_profile_present()) {
        ESP_LOGW(TAG, "audio is permitted WITHOUT a profile. This is the bench exception, "
                      "not a calibration: nothing may be connected to the output.");
    }

    /* The output chain's hardware layer: the mute GPIO and the task that moves
     * it. Nothing before this point has ever driven HK_PIN_DAC_XSMT, which is
     * not a race — hk_pins.h is explicit that the external pull-down is the
     * mechanism that holds it safe through the ROM, the bootloader and all of
     * app init, and this firmware's job is only ever to RELEASE mute. So it is
     * started here, after the boot report has said what it thinks, rather than
     * being hurried in front of it.
     *
     * The verdict is pushed before the task exists, so the first tick already
     * has the real answer instead of the module's own safe default.
     *
     * Not fatal: a speaker whose mute line could not be configured is a
     * speaker that stays quiet, which is the correct outcome, and taking the
     * device down would remove the only way to tell anyone about it. */
    hk_audio_hw_set_permitted(audio_permitted_now());
    {
        const esp_err_t chain_err = hk_audio_hw_start();
        if (chain_err != ESP_OK) {
            ESP_LOGE(TAG, "the output chain did not start: %s. The mute line is left "
                          "to its pull-down and nothing will release it.",
                     esp_err_to_name(chain_err));
        }
    }

#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
    /* The bench instrument, started here rather than where the receiver would
     * be, because it needs no address and no network: the operator gets an
     * answer a second after reset instead of after a Wi-Fi join. hk_airplay is
     * not started at all in this build -- see HK_AIRPLAY_RUNS.
     *
     * The stream_live input is pushed by hand once the tone is confirmed
     * running, which is the same fact the receiver's playback callback pushes
     * and it is pushed for the same reason: the sequence in hk_audio.c releases
     * the DAC mute on `permitted && stream_live`, and a tone generated behind
     * an asserted mute is a tone nobody can hear. It is set AFTER the start
     * call succeeds, so a failed instrument never claims a live stream. */
    {
        const esp_err_t tone_err = hk_tone_start(on_tone_done, NULL);
        if (tone_err != ESP_OK) {
            ESP_LOGE(TAG, "the bench test tone did not start: %s. Nothing is driving "
                          "I2S in this build, so silence proves nothing.",
                     esp_err_to_name(tone_err));
        } else {
            hk_audio_hw_set_stream_live(true);
            if (!audio_permitted_now()) {
                /* The other half of the answer, said before the operator spends
                 * ten minutes listening to a board that was never going to make
                 * a sound. */
                ESP_LOGW(TAG, "the tone is being generated but audio is NOT permitted, "
                              "so the DAC mute stays asserted and you will hear "
                              "nothing. This build also needs "
                              "CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE.");
            }
        }
    }
#endif

    /* The button is read and the LED is driven. The handle is published before
     * the task starts, so the first possible press already has somewhere to
     * send its work. */
    s_main_task = xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(hk_ui_start(on_button, NULL));

    start_network();
    hk_ui_clear_booting();
#if CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
    ESP_LOGW(TAG, "BENCH TONE BUILD: the AirPlay receiver is NOT started, whether or not "
                  "it was compiled in, because it and the tone cannot both own I2S. This "
                  "speaker will not appear on any phone. The tone is written straight to "
                  "I2S and does not pass through hk_dsp: no crossover, no protective "
                  "high-pass and no limiter in front of the amplifiers in this build.");
#if CONFIG_HK_BENCH_SWEEP
    /* Said again here, at the end of the boot report, because this is the last
     * thing on the console before the sweep's own banner and it is the one
     * warning that cannot be enforced anywhere: the amplifier is kept out of
     * the sweep's circuit by the wiring alone. The bench exception symbol that
     * releases the DAC releases it too, so the boot report above saying
     * "PERMITTED" is not evidence that the amplifier is idle. */
    ESP_LOGW(TAG, "SWEEP BUILD: this one is not for listening to. It measures ONE driver "
                  "through a 470 ohm series resistor straight off the DAC line output, "
                  "with the amplifier UNPLUGGED. Check the speaker terminals and the "
                  "amplifier input before the ten second lead-in runs out. The procedure "
                  "is docs/02-hardware/driver-measurements.md.");
#endif
#elif CONFIG_HK_AIRPLAY && CONFIG_HK_AIRPLAY_OUTPUT_DSP && CONFIG_HK_BENCH_PROVISIONAL_PROFILE
    /* Bench wording, behind the bench symbol, for the reason given at the
     * matching line in report_policies(): the product image must not carry
     * a sentence about placeholders, and CI reads the image to make sure. */
    ESP_LOGW(TAG, "the AirPlay receiver is built in, with the DSP chain in its output "
                  "(ADR-0022): EQ, subsonic high-pass, LR4 crossover, supply-budget "
                  "stage and a limiter per branch. BENCH BUILD: with no stored profile "
                  "it runs on the compiled-in bench profile, whose corners, ceilings and "
                  "budget are placeholders until G0/G1/G2 produce a measured one. See "
                  "docs/03-firmware/firmware-plan.md stage F3.");
#elif CONFIG_HK_AIRPLAY && CONFIG_HK_AIRPLAY_OUTPUT_DSP
    ESP_LOGI(TAG, "the AirPlay receiver is built in, with the DSP chain in its output "
                  "(ADR-0022): EQ, subsonic high-pass, LR4 crossover, supply-budget "
                  "stage and a limiter per branch. It runs only on a stored profile this "
                  "boot judged valid; without one the receiver is not started. See "
                  "docs/03-firmware/firmware-plan.md stage F3.");
#elif CONFIG_HK_AIRPLAY && CONFIG_HK_AIRPLAY_OUTPUT_SPDIF
    ESP_LOGW(TAG, "the AirPlay receiver is built in with upstream's S/PDIF output stage, "
                  "which has no DSP in it: no crossover, no protective high-pass and no "
                  "limiter in this build. It is the bring-up devkit's output (ADR-0012), "
                  "one pin into an external DAC with no amplifier behind it; the "
                  "product's output is the DSP backend (ADR-0022).");
#elif CONFIG_HK_AIRPLAY
    ESP_LOGW(TAG, "the AirPlay receiver is built in with upstream's passthrough output "
                  "stage, which has no DSP in it: no crossover, no protective high-pass "
                  "and no limiter in this build. That stage is selectable on the bring-up "
                  "devkit or a bench-exception build only; the product's output is the "
                  "DSP backend (ADR-0022). See docs/03-firmware/firmware-plan.md stage F3.");
#else
    ESP_LOGW(TAG, "no audio in this build. The button, LED and provisioning policy are "
                  "live. See docs/03-firmware/firmware-plan.md for what comes next.");
#endif

    /* The supervisory loop. It exists to give the provisioning policy a clock:
     * a window that closes after ten minutes needs something to notice that
     * ten minutes have passed, and an event-driven system has no event for
     * "nothing happened". One second is far finer than the window needs and
     * costs nothing measurable next to the radios. */
    bool radios_were_open = false;
    while (true) {
        /* One second is the policy's clock. The notification only makes a
         * button press act now instead of up to a second later; nothing depends
         * on it arriving. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        /* The button's work, on a stack that can hold it.
         *
         * Order is not cosmetic: a factory reset asks for both, and user
         * settings must be restored before the credentials are cleared, so a
         * power cut between them leaves a device that has forgotten its network
         * rather than one that kept stale settings it can no longer be told to
         * change. Calibration is in another partition this firmware opens
         * read-only, so neither call can reach it (PRD-008). */
        const uint32_t actions = take_pending_actions();
        if ((actions & HK_ACTION_RESET_USER_SETTINGS) != 0u
            && hk_storage_user_reset() != ESP_OK) {
            ESP_LOGE(TAG, "could not restore user settings");
        }
        if ((actions & HK_ACTION_FORGET_CREDENTIALS) != 0u
            && hk_network_forget_credentials() != ESP_OK) {
            ESP_LOGE(TAG, "could not clear credentials");
        }
        if ((actions & HK_ACTION_OPEN_PROVISIONING) != 0u
            && hk_network_open_provisioning() != ESP_OK) {
            ESP_LOGE(TAG, "could not open provisioning");
        }
#if HK_AIRPLAY_RUNS
        if ((actions & HK_ACTION_START_AIRPLAY) != 0u) {
            /* Failure is logged by hk_airplay and is not fatal: a speaker that
             * cannot receive AirPlay is still a speaker that can be reached,
             * updated and reset, and taking the device down would remove the
             * only way to fix it. */
            (void)hk_airplay_start(on_airplay_state, NULL);
        }
#endif

        /* The gate, on the same clock, to the thing that acts on it.
         *
         * Polled rather than pushed because the gate has no event behind it:
         * whether audio is permitted is a conclusion drawn from storage rather
         * than something that happens. A profile appears when a calibration is
         * written, and one second is far finer than that changes. */
        hk_audio_hw_set_permitted(audio_permitted_now());

        /* Confirm or roll back this image, once, when the evidence is in. */
        hk_health_monitor_tick(now_ms());

        run_update_check();

        prov_event(HK_PROV_EV_TICK);

        const hk_prov_t now = prov_snapshot();
        const hk_prov_radios_t want = hk_prov_radios(&now);
        const bool radios_open = want.ble || want.softap;
        if (radios_were_open && !radios_open) {
            ESP_LOGI(TAG, "provisioning window closed after %s",
                     now.bounded ? "its bounded timeout" : "success");
            if (hk_network_close_provisioning() != ESP_OK) {
                ESP_LOGE(TAG, "could not close provisioning");
            }
        }
        radios_were_open = radios_open;
    }
}
