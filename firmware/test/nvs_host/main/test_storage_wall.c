/*
 * The calibration wall, executed rather than argued.
 *
 * PRD-008 says a user reset must not reach factory_cal. Until now the project
 * defended that structurally — a separate partition, a read-only open, and a
 * CI scanner proving no destructive call names the calibration partition. That
 * is a good argument and it is not a test: it reasons about the source rather
 * than running it.
 *
 * This runs it. Real NVS, the real partition table generated from the same
 * partitions.csv the firmware ships, and the real hk_storage.c — on a host,
 * with no board.
 *
 * Since 2026-09-12 it also runs the gate the wall exists for. Audio is
 * permitted on three things (ADR-0022): the store's schema is understood, a
 * profile blob is present, and hk_main has judged that blob valid with
 * hk_profile_load() — the same function the output backend builds its chain
 * from. The judge is compiled in (see CMakeLists.txt), so what is exercised
 * below is the boot sequence itself: write a profile the way a bench would,
 * read it back through hk_storage's own read-only window, judge the bytes,
 * hand the verdict to hk_storage, and see what the gate says.
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_partition.h"
#include "esp_private/partition_linux.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "hk_profile.h"
#include "hk_schema.h"
#include "hk_storage.h"

#define FLASH_SIZE_BYTES (16 * 1024 * 1024)

/*
 * What hk_main judges at: the receiver's output rate and the adapter of
 * ADR-0020. The firmware takes them from CONFIG_OUTPUT_SAMPLE_RATE_HZ and
 * CONFIG_HK_SUPPLY_MV; this project has no Kconfig of its own, so they are
 * literals here. Nothing under test depends on which chain the judge builds,
 * only on whether it builds one.
 */
#define OUTPUT_RATE_HZ 44100.0f
#define SUPPLY_MV      24000.0f

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                      \
    } while (0)

#define CHECK_ERR(call)                                                        \
    do {                                                                       \
        esp_err_t e_ = (call);                                                 \
        checks++;                                                              \
        if (e_ != ESP_OK) {                                                    \
            failures++;                                                        \
            printf("  FAIL %s:%d  %s -> %s\n", __FILE__, __LINE__, #call,      \
                   esp_err_to_name(e_));                                       \
        }                                                                      \
    } while (0)

/*
 * A profile the judge accepts.
 *
 * NOT DRIVER VALUES. Every number is invented for the arithmetic, the same
 * way firmware/test/test_profile.c invents its fixture, and must never be
 * copied into a device: beyond their DC resistances the Nova woofer and
 * tweeter have not been measured (G0). What matters here is only that
 * hk_profile_load() answers "ok" to these bytes, so the gate can be watched
 * opening on a verdict and closing on the next one.
 */
static hk_profile_t valid_profile(void)
{
    hk_profile_t p;
    memset(&p, 0, sizeof(p));
    p.schema = HK_PROFILE_SCHEMA;
    p.measured_yyyymmdd = 20260912u;
    (void)snprintf(p.source, sizeof(p.source), "wall-test-not-a-measurement");
    p.woofer_dcr_ohm = 6.0f;
    p.tweeter_dcr_ohm = 5.0f;
    p.woofer_hpf_hz = 45.0f;
    p.crossover_hz = 2200.0f;
    p.woofer_gain = 0.9f;
    p.tweeter_gain = 0.7f;
    p.reference_supply_mv = 24000.0f;
    p.woofer_ceiling = 0.8f;
    p.tweeter_ceiling = 0.5f;
    p.woofer_release_ms = 120u;
    p.woofer_hold_ms = 30u;
    p.tweeter_release_ms = 80u;
    p.tweeter_hold_ms = 15u;
    p.woofer_delay_samples = 0u;
    p.tweeter_delay_samples = 0u;
    p.tweeter_polarity = 0u;
    p.supply_budget_sq = 0.3f;
    p.supply_window_ms = 50u;
    p.amp_gain_db = 0u; /* C3 not done: accepted, carried as "unread" */
    return p;
}

/** Put a blob under the profile key, the way a bench would. Any length. */
static void write_profile_bytes(const void *blob, size_t length)
{
    nvs_handle_t handle;
    CHECK_ERR(nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                      HK_STORAGE_FACTORY_NAMESPACE,
                                      NVS_READWRITE, &handle));
    CHECK_ERR(nvs_set_blob(handle, HK_STORAGE_PROFILE_KEY, blob, length));
    CHECK_ERR(nvs_commit(handle));
    nvs_close(handle);
}

/** Put a known value into the calibration store, the way a bench would. */
static void write_calibration(uint32_t marker)
{
    nvs_handle_t handle;
    CHECK_ERR(nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                      HK_STORAGE_FACTORY_NAMESPACE,
                                      NVS_READWRITE, &handle));
    /* The keys hk_storage actually reads. Writing different ones leaves the
     * store looking absent, and the test then proves the wall around an empty
     * partition rather than around a usable calibration. The schema version
     * alone was once enough for the gate, and this test once wrote only that
     * and a marker; since 2026-09-08 the gate wants the profile itself, so a
     * calibration here is a version, a marker and a profile the judge accepts.
     * The marker is what the wall is watched through: a value no firmware
     * path writes, so its survival says the partition was untouched. */
    CHECK_ERR(nvs_set_u32(handle, "schema", HK_SCHEMA_FACTORY_VERSION));
    CHECK_ERR(nvs_set_u32(handle, "marker", marker));
    CHECK_ERR(nvs_commit(handle));
    nvs_close(handle);

    const hk_profile_t profile = valid_profile();
    write_profile_bytes(&profile, sizeof(profile));
}

/** Read it back through a fresh handle, so nothing is cached. */
static bool read_calibration(uint32_t *marker)
{
    nvs_handle_t handle;
    if (nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                HK_STORAGE_FACTORY_NAMESPACE,
                                NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    const bool ok = nvs_get_u32(handle, "marker", marker) == ESP_OK;
    nvs_close(handle);
    return ok;
}

/**
 * What hk_main does with a present profile at boot, in miniature.
 *
 * Read the blob back through hk_storage's own window -- the read-only path
 * the firmware uses, not the test's write handle -- into a buffer of exactly
 * one profile, so a longer blob fails the read instead of being trusted; then
 * judge the bytes with the backend's judge. This mirrors judge_profile() in
 * firmware/main/hk_main.c and should change when it does.
 *
 * @return the read's result; @p verdict is meaningful only when it is ESP_OK.
 */
static esp_err_t judge_stored_profile(hk_profile_verdict_t *verdict)
{
    uint8_t raw[sizeof(hk_profile_t)];
    size_t  length = sizeof(raw);
    const esp_err_t err = hk_storage_factory_get_blob(HK_STORAGE_PROFILE_KEY, raw, &length);
    if (err != ESP_OK) {
        *verdict = HK_PROFILE_BAD_SCHEMA;
        return err;
    }
    *verdict = hk_profile_load(raw, length, OUTPUT_RATE_HZ, SUPPLY_MV, NULL, NULL);
    return ESP_OK;
}

/** Judge, then tell storage -- the two calls hk_main makes after init. */
static hk_profile_verdict_t boot_judge(void)
{
    hk_profile_verdict_t verdict;
    const esp_err_t err = judge_stored_profile(&verdict);
    hk_storage_profile_judged(err == ESP_OK && verdict == HK_PROFILE_OK);
    return verdict;
}

int main(void)
{
    printf("Merzarkabul Airplay Speakers storage wall, on real NVS\n");

    /* Point the linux partition layer at the table the device actually uses.
     * Generating it from partitions.csv at build time is what stops this test
     * drifting into checking offsets nobody ships. */
    esp_partition_file_mmap_ctrl_t *ctrl = esp_partition_get_file_mmap_ctrl_input();
    snprintf(ctrl->partition_file_name, sizeof(ctrl->partition_file_name),
             "%s", "hk-partitions.bin");
    ctrl->flash_file_size = FLASH_SIZE_BYTES;
    ctrl->remove_dump = true;

    /* --- the table really is ours --- */
    const esp_partition_t *cal = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS,
        HK_STORAGE_FACTORY_PARTITION);
    CHECK(cal != NULL);
    if (cal != NULL) {
        printf("  factory_cal at 0x%06" PRIx32 ", %" PRIu32 " bytes\n",
               cal->address, cal->size);
        CHECK(cal->address == 0x13000);
    }
    const esp_partition_t *user = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    CHECK(user != NULL);
    if (user != NULL) {
        CHECK(user->address == 0x9000);
        /* They are different partitions, which is the whole basis of the wall. */
        CHECK(user->address != cal->address);
    }

    /* --- bring both stores up the way the firmware does --- */
    CHECK_ERR(nvs_flash_init());
    CHECK_ERR(nvs_flash_init_partition(HK_STORAGE_FACTORY_PARTITION));

    write_calibration(0xC0FFEE);
    uint32_t marker = 0;
    CHECK(read_calibration(&marker));
    CHECK(marker == 0xC0FFEE);

    /* A user setting to lose. */
    CHECK_ERR(hk_storage_init());

    /* --- the gate, in the order the boot walks it ---
     * With a calibration present and its schema understood, the store knows
     * the profile is there. That is not yet permission: the blob has not been
     * judged, and a present profile that nobody has judged is exactly the
     * blob that used to open the DAC into a chain the backend had refused. */
    CHECK(hk_storage_factory_action() == HK_SCHEMA_USE);
    CHECK(hk_storage_profile_present());
    CHECK(!hk_storage_profile_valid());
    CHECK(!hk_storage_audio_permitted());

    /* The judge reads the same bytes the bench wrote and accepts them; the
     * verdict is what opens the gate. This is what the wall is protecting:
     * not an empty partition, a working one. */
    CHECK(boot_judge() == HK_PROFILE_OK);
    CHECK(hk_storage_profile_valid());
    CHECK(hk_storage_audio_permitted());

    /* And the gate obeys the verdict, not the presence: told the same blob was
     * refused, it shuts; told again it was accepted, it opens. */
    hk_storage_profile_judged(false);
    CHECK(!hk_storage_profile_valid());
    CHECK(!hk_storage_audio_permitted());
    hk_storage_profile_judged(true);
    CHECK(hk_storage_audio_permitted());

    CHECK_ERR(hk_storage_user_set_u32("volume", 77));
    uint32_t volume = 0;
    CHECK(hk_storage_user_read_u32("volume", &volume));
    CHECK(volume == 77);

    /* --- the reset --- */
    printf("  running hk_storage_user_reset()\n");
    CHECK_ERR(hk_storage_user_reset());

    /* The user setting is gone: that is what a reset is for. */
    volume = 0;
    CHECK(!hk_storage_user_read_u32("volume", &volume));

    /* And the calibration is not. This is PRD-008, executed: the marker is
     * still there, the profile still reads back and still judges valid, and
     * the verdict the boot reached was not touched by the reset either --
     * the speaker that was permitted to play before the reset still is. */
    marker = 0;
    CHECK(read_calibration(&marker));
    CHECK(marker == 0xC0FFEE);
    {
        hk_profile_verdict_t verdict;
        CHECK_ERR(judge_stored_profile(&verdict));
        CHECK(verdict == HK_PROFILE_OK);
    }
    CHECK(hk_storage_profile_valid());
    CHECK(hk_storage_audio_permitted());

    /* Which means the speaker can still play after a factory reset — the point
     * of keeping the two apart. A device that lost its driver protection every
     * time someone reset their settings would be worse than one with no reset.
     *
     * Bringing the stores up again forgets the verdict on purpose: a verdict
     * belongs to the bytes it was reached on, and a fresh init has re-read
     * what is there. So the profile is present and, until the judge is asked
     * again, permits nothing; asked again, it answers the same. */
    CHECK_ERR(hk_storage_init());
    CHECK(hk_storage_factory_action() == HK_SCHEMA_USE);
    CHECK(hk_storage_profile_present());
    CHECK(!hk_storage_profile_valid());
    CHECK(!hk_storage_audio_permitted());
    CHECK(boot_judge() == HK_PROFILE_OK);
    CHECK(hk_storage_audio_permitted());

    /* --- and again, because once could be luck --- */
    for (int i = 0; i < 20; i++) {
        CHECK_ERR(hk_storage_user_set_u32("volume", (uint32_t)i));
        CHECK_ERR(hk_storage_user_reset());
        marker = 0;
        CHECK(read_calibration(&marker));
        CHECK(marker == 0xC0FFEE);
        hk_profile_verdict_t verdict;
        CHECK_ERR(judge_stored_profile(&verdict));
        CHECK(verdict == HK_PROFILE_OK);
    }

    /* --- the calibration store is opened read-only, so a write through the
     *     firmware's own path must fail rather than succeed quietly --- */
    {
        nvs_handle_t handle;
        const esp_err_t err = nvs_open_from_partition(
            HK_STORAGE_FACTORY_PARTITION, HK_STORAGE_FACTORY_NAMESPACE,
            NVS_READONLY, &handle);
        CHECK_ERR(err);
        if (err == ESP_OK) {
            CHECK(nvs_set_u32(handle, "marker", 0xDEAD) != ESP_OK);
            nvs_close(handle);
        }
        marker = 0;
        CHECK(read_calibration(&marker));
        CHECK(marker == 0xC0FFEE);
    }

    /* --- a present profile the judge refuses does not permit audio ---
     *     The bench can write a bad one; this firmware cannot mend it. Three
     *     shapes: a blob of the right length with an impossible number in it,
     *     refused by name; a blob of a schema-1 length, refused as not a
     *     profile at all; and a blob longer than a profile, which fails the
     *     read before the judge sees it. Each leaves the gate shut. Whether
     *     the bench build's exception would open it is not something this
     *     binary proves: that branch is behind a project Kconfig symbol this
     *     test has no Kconfig for, so it is never compiled here. By inspection
     *     of hk_storage.c it lifts absence only, and none of these is
     *     absent. */
    {
        hk_profile_t bad = valid_profile();
        bad.tweeter_ceiling = 0.0f; /* a tweeter with no ceiling is no protection */
        write_profile_bytes(&bad, sizeof(bad));
        CHECK_ERR(hk_storage_init());
        CHECK(hk_storage_profile_present());
        CHECK(boot_judge() == HK_PROFILE_BAD_CEILING);
        CHECK(!hk_storage_profile_valid());
        CHECK(!hk_storage_audio_permitted());

        /* Schema 1 was 84 bytes on the wire. Nothing ever wrote one to a
         * device, and this is what one would meet if it had. */
        uint8_t short_blob[84];
        memset(short_blob, 0, sizeof(short_blob));
        short_blob[0] = 1u; /* schema 1, little-endian uint16 */
        write_profile_bytes(short_blob, sizeof(short_blob));
        CHECK_ERR(hk_storage_init());
        CHECK(hk_storage_profile_present());
        CHECK(boot_judge() == HK_PROFILE_BAD_SCHEMA);
        CHECK(!hk_storage_audio_permitted());

        uint8_t long_blob[sizeof(hk_profile_t) + 4u];
        memset(long_blob, 0, sizeof(long_blob));
        write_profile_bytes(long_blob, sizeof(long_blob));
        CHECK_ERR(hk_storage_init());
        CHECK(hk_storage_profile_present());
        {
            hk_profile_verdict_t verdict;
            CHECK(judge_stored_profile(&verdict) == ESP_ERR_NVS_INVALID_LENGTH);
        }
        CHECK(boot_judge() != HK_PROFILE_OK);
        CHECK(!hk_storage_audio_permitted());

        /* The bench writes a good one again, and the next boot is permitted.
         * The marker never moved through any of this: the profile key was
         * rewritten by the test's own bench handle, not by the firmware. */
        const hk_profile_t good = valid_profile();
        write_profile_bytes(&good, sizeof(good));
        CHECK_ERR(hk_storage_init());
        CHECK(boot_judge() == HK_PROFILE_OK);
        CHECK(hk_storage_audio_permitted());
        marker = 0;
        CHECK(read_calibration(&marker));
        CHECK(marker == 0xC0FFEE);
    }

    /* --- a full default-partition erase still spares it ---
     *     nvs_flash_erase() names no partition and takes the default one. If
     *     factory_cal ever shared that partition, this is where it would die. */
    CHECK_ERR(nvs_flash_erase());
    marker = 0;
    CHECK(read_calibration(&marker));
    CHECK(marker == 0xC0FFEE);
    {
        hk_profile_verdict_t verdict;
        CHECK_ERR(judge_stored_profile(&verdict));
        CHECK(verdict == HK_PROFILE_OK);
    }

    esp_partition_file_munmap();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
