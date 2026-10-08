/* Host-side tests of the production v2 image header and CRC validator. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "image_integrity.h"

#define MAP_ADDRESS 0x10000000UL
#define MAP_LENGTH  0x00080000UL
#define APP_OFFSET  0x00006000UL
#define IMAGE_LEN   0x00003000UL
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); \
} } while (0)

static uint8_t *image;
static uintptr_t first;
static uintptr_t end;
static unsigned tests;

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t crc32_bytes(const uint8_t *data, unsigned len) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (unsigned i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8U; ++bit)
            crc = (crc >> 1) ^
                (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}
static uint32_t crc32_image(const uint8_t *data, unsigned len) {
    /* Independent fixture-side implementation over two byte spans. */
    uint32_t crc = UINT32_C(0xffffffff);
    for (unsigned i = 0; i < len; ++i) {
        if (i >= AM13E_IMAGE_HEADER_OFFSET &&
            i < AM13E_IMAGE_HEADER_OFFSET + AM13E_IMAGE_HEADER_SIZE)
            continue;
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8U; ++bit)
            crc = (crc >> 1) ^
                (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}
static void seal_header(void) {
    uint8_t *h = image + AM13E_IMAGE_HEADER_OFFSET;
    put32(h + 28U, crc32_bytes(h, 28U));
}
static void fixture(void) {
    memset(image, 0xff, (size_t)(end - first));
    for (unsigned i = 0; i < IMAGE_LEN; ++i)
        image[i] = (uint8_t)(i * 13U + 7U);
    image[0] = 0xea;
    image[1] = 0x32;
    put32(image + AM13E_IMAGE_VECTOR_OFFSET, UINT32_C(0x20001000));
    put32(image + AM13E_IMAGE_VECTOR_OFFSET + 4U,
          AM13E_IMAGE_APP_BASE + AM13E_IMAGE_VECTOR_OFFSET + 0x80U + 1U);
    uint8_t *h = image + AM13E_IMAGE_HEADER_OFFSET;
    put32(h + 0U, AM13E_IMAGE_MAGIC);
    put16(h + 4U, AM13E_IMAGE_HEADER_VERSION);
    put16(h + 6U, AM13E_IMAGE_HEADER_SIZE);
    put32(h + 8U, AM13E_IMAGE_TARGET);
    put32(h + 12U, IMAGE_LEN);
    put32(h + 16U, 0U);
    put32(h + 20U, 0U);
    put32(h + 24U, 0U);
    put32(h + 16U, crc32_image(image, IMAGE_LEN));
    seal_header();
}
static boot_am13e_image_status_t check(void) {
    return boot_am13e_image_check(first, end, NULL, 0U, NULL);
}
static void run_tests(void) {
    uint32_t length = 0U;
    fixture();
    CHECK(boot_am13e_image_check(first, end, NULL, 0U, &length)
          == AM13E_IMAGE_VALID);
    CHECK(length == IMAGE_LEN);
    puts("PASS valid v2 E62 image metadata and CRC"); ++tests;

    fixture();
    image[0x1000U] ^= 0x10U;
    CHECK(check() == AM13E_IMAGE_INVALID_PAYLOAD_CRC);
    puts("PASS corrupted application data rejected"); ++tests;

    fixture();
    image[AM13E_IMAGE_HEADER_OFFSET + 28U] ^= 1U;
    CHECK(check() == AM13E_IMAGE_INVALID_HEADER_CRC);
    puts("PASS header CRC corruption rejected"); ++tests;

    fixture();
    put32(image + AM13E_IMAGE_HEADER_OFFSET + 8U, UINT32_C(0x12345678));
    CHECK(check() == AM13E_IMAGE_INVALID_HEADER);
    puts("PASS wrong target rejected"); ++tests;

    fixture();
    put32(image + AM13E_IMAGE_HEADER_OFFSET + 12U,
          AM13E_IMAGE_MAX_TRANSPORT_BYTES + 16U);
    seal_header();
    CHECK(check() == AM13E_IMAGE_INVALID_LENGTH);
    puts("PASS unaddressable image length rejected"); ++tests;

    fixture();
    put32(image + AM13E_IMAGE_VECTOR_OFFSET + 4U,
          AM13E_IMAGE_APP_BASE + IMAGE_LEN + 0x80U + 1U);
    CHECK(check() == AM13E_IMAGE_INVALID_VECTOR);
    puts("PASS reset entry outside validated image rejected"); ++tests;

    fixture();
    uint8_t staged[16];
    memcpy(staged, image, sizeof staged);
    memset(image, 0xff, sizeof staged);
    CHECK(check() == AM13E_IMAGE_INVALID_SIGNATURE);
    CHECK(boot_am13e_image_check(first, end, staged, sizeof staged, &length)
          == AM13E_IMAGE_VALID);
    puts("PASS deferred-signature prefix validation"); ++tests;

    fixture();
    memset(image + 0x1400U, 0xff, 0x100U);
    CHECK(check() == AM13E_IMAGE_INVALID_PAYLOAD_CRC);
    puts("PASS truncated/program-missing contents rejected"); ++tests;

    CHECK(crc32_bytes((const uint8_t *)"123456789", 9U)
          == UINT32_C(0xcbf43926));
    puts("PASS CRC-32/ISO-HDLC golden vector"); ++tests;

    printf("PASS %u image integrity validation tests\n", tests);
}
int main(void) {
    void *mapped = mmap((void *)MAP_ADDRESS, MAP_LENGTH,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        return 2;
    }
    first = MAP_ADDRESS + APP_OFFSET;
    end = MAP_ADDRESS + MAP_LENGTH;
    image = (uint8_t *)first;
    run_tests();
    return 0;
}
