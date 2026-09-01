/*
 * The bridge between the release tooling and the firmware.
 *
 * make_manifest.py writes a manifest; hk_manifest_validate() decides whether a
 * device may install what it describes. Until this existed the two were only
 * ever compared by FIELD NAME — a mismatch in a VALUE (a version the parser
 * spells differently, a digest in the wrong case, a size that overflows) would
 * have passed every test in the repository and been discovered by four
 * speakers refusing every release.
 *
 * So this reads a real generated manifest, runs the real parser and the real
 * validator over it, and reports what a device would have concluded.
 *
 *   manifest_e2e <manifest.json> <product> <target> <hw_revision> <channel>
 *                <running_version> <running_secure_version> <slot_size>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hk_manifest.h"
#include "hk_ota.h"

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    const long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    char *buffer = malloc((size_t)size + 1u);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }
    const size_t got = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    buffer[got] = '\0';
    return buffer;
}

int main(int argc, char **argv)
{
    if (argc != 9) {
        fprintf(stderr, "usage: %s <manifest.json> <product> <target> "
                        "<hw_revision> <channel> <running_version> "
                        "<running_secure_version> <slot_size>\n", argv[0]);
        return 2;
    }

    char *json = read_all(argv[1]);
    if (json == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    hk_manifest_t manifest;
    if (!hk_manifest_parse(json, &manifest)) {
        printf("PARSE_FAILED\n");
        free(json);
        return 1;
    }
    free(json);

    const hk_device_t device = {
        .product = argv[2],
        .target = argv[3],
        .hw_revision = argv[4],
        .channel = argv[5],
        .running_version = argv[6],
        .running_secure_version = (uint32_t)strtoul(argv[7], NULL, 0),
        .slot_size = (uint32_t)strtoul(argv[8], NULL, 0),
    };

    /* Report which fields survived parsing, so a missing one is diagnosable
     * rather than just "FIELD_MISSING". */
    printf("present=0x%03x\n", (unsigned)manifest.present);
    printf("version=%s\n", manifest.version);
    printf("product=%s\n", manifest.product);
    printf("asset_len=%u\n", (unsigned)strlen(manifest.asset));

    const hk_manifest_err_t verdict = hk_manifest_validate(&manifest, &device);
    printf("%s\n", hk_manifest_err_name(verdict));
    return verdict == HK_MANIFEST_OK ? 0 : 1;
}
