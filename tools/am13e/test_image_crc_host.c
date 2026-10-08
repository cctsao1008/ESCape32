/* Host cross-check of E62 image format using the real Boot CRC implementation.
 * This exercises boot/src/crc32.c; it is NOT an execution of boot_image.c.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crc32.h"

#define MAX_APP_SIZE 0x7A000u
#define HEADER_OFFSET 0x100u
#define HEADER_SIZE 32u
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t u16(const uint8_t *p) {
    return (uint16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s packed-image\n", argv[0]);
        return 2;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("open"); return 2; }
    if (fseek(fp, 0, SEEK_END)) { fclose(fp); return 2; }
    long n = ftell(fp);
    if (n < 0 || (unsigned long)n > MAX_APP_SIZE ||
        n < HEADER_OFFSET + HEADER_SIZE) { fclose(fp); return 1; }
    rewind(fp);
    uint8_t *bytes = malloc((size_t)n);
    if (!bytes) { fclose(fp); return 2; }
    if (fread(bytes, 1, (size_t)n, fp) != (size_t)n) {
        free(bytes); fclose(fp); return 2;
    }
    fclose(fp);
    const uint8_t *h = bytes + HEADER_OFFSET;
    uint32_t length = u32(h + 12);
    int ok = u32(h) == 0x49323645u && u16(h + 4) == 1u &&
        u16(h + 6) == HEADER_SIZE && u32(h + 8) == 0x33314D41u &&
        length >= HEADER_OFFSET + HEADER_SIZE &&
        length <= (uint32_t)n && length <= MAX_APP_SIZE &&
        !(length & 15u) && u32(h + 20) == 0u &&
        u32(h + 24) == 0u &&
        boot_crc32(h, 28u) == u32(h + 28);
    if (ok) {
        uint32_t state = boot_crc32_begin();
        state = boot_crc32_update(state, bytes, HEADER_OFFSET);
        state = boot_crc32_update(state,
            bytes + HEADER_OFFSET + HEADER_SIZE,
            length - HEADER_OFFSET - HEADER_SIZE);
        ok = boot_crc32_end(state) == u32(h + 16);
    }
    free(bytes);
    if (!ok) { fputs("[FAIL] C Boot CRC / image format\n", stderr); return 1; }
    puts("[PASS] C Boot CRC / image format");
    return 0;
}
