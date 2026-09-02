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
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_partition.h"
#include "esp_private/partition_linux.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "hk_schema.h"
#include "hk_storage.h"

#define FLASH_SIZE_BYTES (16 * 1024 * 1024)

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

/** Put a known value into the calibration store, the way a bench would. */
static void write_calibration(uint32_t marker)
{
    nvs_handle_t handle;
    CHECK_ERR(nvs_open_from_partition(HK_STORAGE_FACTORY_PARTITION,
                                      HK_STORAGE_FACTORY_NAMESPACE,
                                      NVS_READWRITE, &handle));
    /* The key hk_storage actually reads. Writing a different one leaves the
     * store looking absent, and the test then proves the wall around an empty
     * partition rather than around a usable calibration. */
    CHECK_ERR(nvs_set_u32(handle, "schema", HK_SCHEMA_FACTORY_VERSION));
    CHECK_ERR(nvs_set_u32(handle, "marker", marker));
    CHECK_ERR(nvs_commit(handle));
    nvs_close(handle);
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

int main(void)
{
    printf("Harman Kardom storage wall, on real NVS\n");

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

    /* With a calibration present and its schema understood, the store reports
     * a usable profile and audio is allowed. This is what the wall is
     * protecting: not an empty partition, a working one. */
    CHECK(hk_storage_factory_action() == HK_SCHEMA_USE);
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

    /* And the calibration is not. This is PRD-008, executed. */
    marker = 0;
    CHECK(read_calibration(&marker));
    CHECK(marker == 0xC0FFEE);

    /* Which means the speaker can still play after a factory reset — the point
     * of keeping the two apart. A device that lost its driver protection every
     * time someone reset their settings would be worse than one with no reset. */
    CHECK_ERR(hk_storage_init());
    CHECK(hk_storage_factory_action() == HK_SCHEMA_USE);
    CHECK(hk_storage_audio_permitted());

    /* --- and again, because once could be luck --- */
    for (int i = 0; i < 20; i++) {
        CHECK_ERR(hk_storage_user_set_u32("volume", (uint32_t)i));
        CHECK_ERR(hk_storage_user_reset());
        marker = 0;
        CHECK(read_calibration(&marker));
        CHECK(marker == 0xC0FFEE);
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

    /* --- a full default-partition erase still spares it ---
     *     nvs_flash_erase() names no partition and takes the default one. If
     *     factory_cal ever shared that partition, this is where it would die. */
    CHECK_ERR(nvs_flash_erase());
    marker = 0;
    CHECK(read_calibration(&marker));
    CHECK(marker == 0xC0FFEE);

    esp_partition_file_munmap();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
