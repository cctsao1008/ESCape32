/* Execute production boot_image.c against host-backed APP memory.
 * Header include guards allow remapping the APP addresses in this test TU.
 * This is a host-only harness; production sources are not changed.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image_contract.h"
#include "boot_image.h"
#include "crc32.h"

static uint8_t app_image[ESCAPE32_APP_SIZE] __attribute__((aligned(16)));
#define TEST_APP_SIZE ESCAPE32_APP_SIZE
#undef ESCAPE32_APP_BASE
#define ESCAPE32_APP_BASE ((uintptr_t)app_image)
#undef ESCAPE32_IMAGE_HEADER_BASE
#define ESCAPE32_IMAGE_HEADER_BASE (ESCAPE32_APP_BASE + ESCAPE32_IMAGE_HEADER_OFFSET)
/* Pull in the actual production implementation under host memory mapping. */
#include "../../boot/mcu/AM13E23019/src/boot_image.c"

static int vector_valid = 1;
bool boot_port_app_valid(void) { return vector_valid != 0; }

static void refresh_header_crc(void) {
    escape32_image_header_t *h =
        (escape32_image_header_t *)(void *)(app_image + ESCAPE32_IMAGE_HEADER_OFFSET);
    h->header_crc32 = boot_crc32(h, 28);
}

static void expect(boot_image_status_t status, const char *name) {
    boot_image_status_t got = boot_image_validate();
    if (got != status) {
        fprintf(stderr, "[FAIL] %s (got %d; expected %d)\n", name, got, status);
        exit(1);
    }
    assert(boot_image_launchable() == (status == BOOT_IMAGE_FULLY_VALID));
    printf("[PASS] %s\n", name);
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s packed.e62.bin\n", argv[0]); return 2; }
    memset(app_image, 0xff, sizeof(app_image));
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("image"); return 2; }
    size_t n = fread(app_image, 1, sizeof(app_image), f);
    if (ferror(f) || n < ESCAPE32_IMAGE_HEADER_OFFSET + ESCAPE32_IMAGE_HEADER_SIZE ||
        fgetc(f) != EOF) {
        fclose(f);
        fprintf(stderr, "[FAIL] bad input image\n");
        return 2;
    }
    fclose(f);
    escape32_image_header_t *h =
        (escape32_image_header_t *)(void *)(app_image + ESCAPE32_IMAGE_HEADER_OFFSET);

    expect(BOOT_IMAGE_FULLY_VALID, "production C image validator accepts packed image");

    vector_valid = 0;
    expect(BOOT_IMAGE_INVALID_VECTOR, "invalid vector rejected");
    vector_valid = 1;

    uint32_t old = h->magic;
    h->magic = 0;
    expect(BOOT_IMAGE_INVALID_HEADER, "invalid magic rejected");
    h->magic = old;

    old = h->target_id;
    h->target_id ^= 1;
    refresh_header_crc();
    expect(BOOT_IMAGE_INVALID_HEADER, "wrong target rejected");
    h->target_id = old;
    refresh_header_crc();

    old = h->image_length;
    h->image_length = TEST_APP_SIZE + 16u;
    refresh_header_crc();
    expect(BOOT_IMAGE_INVALID_HEADER, "out-of-bounds length rejected");
    h->image_length = old;
    refresh_header_crc();

    h->header_crc32 ^= 1;
    expect(BOOT_IMAGE_INVALID_HEADER_CRC, "invalid header CRC rejected");
    h->header_crc32 ^= 1;

    app_image[512] ^= 1;
    expect(BOOT_IMAGE_INVALID_IMAGE_CRC, "invalid payload CRC rejected");
    app_image[512] ^= 1;

    expect(BOOT_IMAGE_FULLY_VALID, "valid image restored");
    puts("[PASS] actual boot_image_validate() host regression");
    return 0;
}
