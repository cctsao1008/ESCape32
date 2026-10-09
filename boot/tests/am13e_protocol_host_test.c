/*
 * Stage D1: wire-level ESCape32 command framing through the real
 * boot/src/main.c dispatcher, boot/src/io.c framing helpers,
 * boot/mcu/AM13E/flash_range.c and production flash.c state machine.
 *
 * The receive/send transport and Flash controller are MOCKS, not PB14 hardware.
 * Host CRC is a software model of CRC-32/ISO-HDLC, not TI CRCP qualification.
 */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>

#include "dl_flash.h"
#include "image_integrity.h"

#define MAP_ADDRESS UINT32_C(0x10000000)
#define MAP_LENGTH  UINT32_C(0x00080000)
#define APP_OFFSET  UINT32_C(0x00006000)
#define BLOCK_BYTES 1024U
#define CMD_PROBE 0U
#define CMD_INFO 1U
#define CMD_READ 2U
#define CMD_WRITE 3U
#define CMD_UPDATE 4U
#define CMD_SETWRP 5U
#define RES_OK 0U
#define RES_ERROR 1U
#define RX_CAPACITY (AM13E_IMAGE_MAX_TRANSPORT_BYTES + 32768U)
#define TX_CAPACITY 8192U
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while (0)

uintptr_t boot_am13e_test_first;
uintptr_t boot_am13e_test_end;
extern void boot_am13e_protocol_entry(void);
extern void boot_am13e_test_reset_update_state(void);

static uint8_t incoming[RX_CAPACITY];
static uint8_t transmitted[TX_CAPACITY];
static uint8_t expected_reply[TX_CAPACITY];
static size_t incoming_length, read_position, transmitted_length, expected_length;
static unsigned erases, programs;
static jmp_buf end_of_stream;

static void input_bytes(const uint8_t *bytes, size_t n) {
    CHECK(n <= sizeof incoming - incoming_length);
    memcpy(incoming + incoming_length, bytes, n);
    incoming_length += n;
}
static void expected_bytes(const uint8_t *bytes, size_t n) {
    CHECK(n <= sizeof expected_reply - expected_length);
    memcpy(expected_reply + expected_length, bytes, n);
    expected_length += n;
}
static void input_val(uint8_t val) {
    uint8_t b[2] = {val, (uint8_t)~val};
    input_bytes(b, sizeof b);
}
static void expected_val(uint8_t val) {
    uint8_t b[2] = {val, (uint8_t)~val};
    expected_bytes(b, sizeof b);
}
static uint32_t software_crc(const uint8_t *buffer, size_t n) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0; i < n; ++i) {
        crc ^= buffer[i];
        for (unsigned bit = 0; bit < 8U; ++bit)
            crc = (crc >> 1U) ^
                  (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}
static void little_endian_32(uint32_t value, uint8_t *out) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}
static void queue_write(unsigned block, const uint8_t *payload,
                        unsigned length, bool wrong_crc, int expected_ack) {
    CHECK(block <= 255U && length >= 4U && length <= BLOCK_BYTES &&
          (length & 3U) == 0U);
    input_val(CMD_WRITE);
    input_val((uint8_t)block);
    input_val((uint8_t)(length / 4U - 1U));
    input_bytes(payload, length);
    uint8_t crc[4];
    uint32_t value = software_crc(payload, length);
    if (wrong_crc) value ^= 1U;
    little_endian_32(value, crc);
    input_bytes(crc, 4U);
    if (expected_ack >= 0) expected_val((uint8_t)expected_ack);
}
static void expected_data(const uint8_t *data, unsigned n) {
    CHECK((n & 3U) == 0U && n >= 4U && n <= BLOCK_BYTES);
    expected_val((uint8_t)(n / 4U - 1U));
    expected_bytes(data, n);
    uint8_t checksum[4];
    little_endian_32(software_crc(data, n), checksum);
    expected_bytes(checksum, sizeof checksum);
}
static void queue_read(unsigned block, unsigned length,
                       const uint8_t *expected_payload) {
    CHECK(block <= 255U && (length & 3U) == 0U && length > 0U);
    input_val(CMD_READ);
    input_val((uint8_t)block);
    input_val((uint8_t)(length / 4U - 1U));
    expected_data(expected_payload, length);
}

/* The next two functions are the actual framing layer's physical backend. */
int recvbuf(char *buf, int len) {
    CHECK(buf != NULL && len >= 0);
    if (read_position == incoming_length) longjmp(end_of_stream, 1);
    CHECK((size_t)len <= incoming_length - read_position);
    memcpy(buf, incoming + read_position, (size_t)len);
    read_position += (size_t)len;
    return 1;
}
void sendbuf(const char *buf, int len) {
    CHECK(buf != NULL && len > 0);
    CHECK((size_t)len <= sizeof transmitted - transmitted_length);
    memcpy(transmitted + transmitted_length, buf, (size_t)len);
    transmitted_length += (size_t)len;
}
/* The host emulates ESCape32 command CRC; hardware CRCP still unqualified. */
uint32_t crc32(const char *buf, int len) {
    CHECK(buf != NULL && len >= 0);
    return software_crc((const uint8_t *)(const void *)buf, (size_t)len);
}
void init(void) {}
void initio(void) {}
bool boot_am13e_take_reboot_ack(void) { return false; }
uint32_t boot_am13e_device_id(void) { return UINT32_C(0x12345678); }
uint8_t boot_am13e_io_id(void) { return 4U; }
bool boot_am13e_application_valid(void) { return false; }
__attribute__((noreturn)) void boot_am13e_launch_application(void) {
    fprintf(stderr, "FAIL unexpected application jump during protocol test\n");
    exit(1);
}

/* Physical Flash model: 2 KiB sector erase, irreversible 1->0 program. */
uint32_t DL_Flash_eraseSector(uint32_t addr) {
    if ((addr & (DL_FLASH_SECTOR_SIZE - 1U)) ||
        addr < boot_am13e_test_first ||
        addr > boot_am13e_test_end - DL_FLASH_SECTOR_SIZE)
        return DL_FLASH_ERROR;
    ++erases;
    memset((void *)(uintptr_t)addr, 0xff, DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}
uint32_t DL_Flash_program(uint32_t addr, uint8_t *src, uint32_t length) {
    if (!src || !length || (addr & 15U) || (length & 15U) ||
        addr < boot_am13e_test_first ||
        addr > boot_am13e_test_end - length)
        return DL_FLASH_ERROR;
    uint8_t *dst = (uint8_t *)(uintptr_t)addr;
    for (uint32_t i = 0U; i < length; ++i)
        if ((uint8_t)(dst[i] & src[i]) != src[i])
            return DL_FLASH_ERROR;
    ++programs;
    for (uint32_t i = 0U; i < length; ++i) dst[i] &= src[i];
    return DL_FLASH_SUCCESS;
}

static uint8_t *load_image(const char *filename, size_t *size_out) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror(filename); exit(2); }
    CHECK(fseek(fp, 0, SEEK_END) == 0);
    const long n = ftell(fp);
    CHECK(n >= (long)(AM13E_IMAGE_VECTOR_OFFSET + 16U) &&
          n <= (long)AM13E_IMAGE_MAX_TRANSPORT_BYTES &&
          (n & 15L) == 0L);
    CHECK(fseek(fp, 0, SEEK_SET) == 0);
    uint8_t *image = malloc((size_t)n);
    CHECK(image != NULL);
    CHECK(fread(image, 1U, (size_t)n, fp) == (size_t)n);
    CHECK(fclose(fp) == 0);
    CHECK(image[0] == 0xea && image[1] == 0x32);
    *size_out = (size_t)n;
    return image;
}
static void queue_protocol(size_t image_bytes, const uint8_t *image) {
    const uint8_t erased[8] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    input_val(CMD_PROBE);
    expected_val(RES_OK);

    input_val(CMD_INFO);
    uint8_t info[32] = {4U, 4U, 0x78U, 0x56U, 0x34U, 0x12U};
    expected_data(info, sizeof info);

    /* A data block before invalidation must return an explicit NAK. */
    queue_write(2U, image + 2048U, 32U, false, RES_ERROR);

    queue_write(0U, erased, sizeof erased, false, RES_OK);
    queue_write(1U, erased, sizeof erased, false, RES_OK);

    /* Valid framing but incorrect transaction block order -> NAK. */
    queue_write(4U, image + 2048U, 32U, false, RES_ERROR);

    /* A bad payload CRC is handled by the shared protocol's goto done:
     * no ACK is emitted, and the write never reaches flash.c.
     * The next command must still be decoded successfully.
     */
    queue_write(2U, image + 2048U, 32U, true, -1);
    uint8_t erased_16[16];
    memset(erased_16, 0xff, sizeof erased_16);
    queue_read(2U, sizeof erased_16, erased_16);

    /* Full 1024-byte transport frames explicitly test count=0xff. */
    for (size_t offset = 2048U; offset < image_bytes; offset += BLOCK_BYTES) {
        unsigned count = (unsigned)((image_bytes - offset > BLOCK_BYTES)
                               ? BLOCK_BYTES : image_bytes - offset);
        queue_write((unsigned)(offset / BLOCK_BYTES), image + offset,
                    count, false, RES_OK);
        /* Retry the last successfully programmed data block. */
        if (offset == 2048U)
            queue_write((unsigned)(offset / BLOCK_BYTES), image + offset,
                        count, false, RES_OK);
    }

    /* Block 0 is staged, so the signature still cannot be read from Flash. */
    queue_write(0U, image, BLOCK_BYTES, false, RES_OK);
    uint8_t pending_head[32];
    memset(pending_head, 0xff, 16U);
    memcpy(pending_head + 16U, image + 16U, 16U);
    queue_read(0U, 32U, pending_head);

    /* Final metadata block commits signature only after complete CRC. */
    queue_write(1U, image + BLOCK_BYTES, BLOCK_BYTES, false, RES_OK);
    queue_write(0U, image, BLOCK_BYTES, false, RES_OK);
    queue_write(1U, image + BLOCK_BYTES, BLOCK_BYTES, false, RES_OK);

    queue_read(0U, 32U, image);
    input_val(CMD_UPDATE); expected_val(RES_ERROR);
    input_val(CMD_SETWRP); input_val(0x33U); expected_val(RES_ERROR);
    input_val(CMD_PROBE); expected_val(RES_OK);
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path/to/AM13E_APP_SMOKE.e62v2.bin\n", argv[0]);
        return 2;
    }
    size_t image_length = 0U;
    uint8_t *image = load_image(argv[1], &image_length);
    void *mapped = mmap((void *)(uintptr_t)MAP_ADDRESS, MAP_LENGTH,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    if (mapped == MAP_FAILED) { perror("mmap"); free(image); return 2; }
    boot_am13e_test_first = MAP_ADDRESS + APP_OFFSET;
    boot_am13e_test_end = MAP_ADDRESS + MAP_LENGTH;
    memset((void *)(uintptr_t)boot_am13e_test_first, 0xff,
           (size_t)(boot_am13e_test_end - boot_am13e_test_first));
    boot_am13e_test_reset_update_state();

    queue_protocol(image_length, image);
    if (setjmp(end_of_stream) == 0) {
        boot_am13e_protocol_entry();
        CHECK(false && "boot protocol unexpectedly returned");
    }

    CHECK(read_position == incoming_length);
    CHECK(transmitted_length == expected_length);
    CHECK(memcmp(transmitted, expected_reply, expected_length) == 0);
    puts("PASS CMD_PROBE/CMD_INFO/CRC32 framing, complement encoding and ACK/NAK");
    puts("PASS malformed payload CRC discarded without Flash programming");
    puts("PASS actual CMD_WRITE ordering, duplicate block and 1024-byte count=0xff");
    puts("PASS signature held erased until final metadata CRC verification");
    CHECK(erases >= 2U && programs >= 4U);

    CHECK(memcmp((const void *)boot_am13e_test_first, image,
                 image_length) == 0);
    uint32_t validated_length = 0U;
    CHECK(boot_am13e_image_check(boot_am13e_test_first, boot_am13e_test_end,
                                 NULL, 0U, &validated_length) ==
          AM13E_IMAGE_VALID);
    CHECK(validated_length == (uint32_t)image_length);
    puts("PASS framed protocol programs ARM-linked packed image byte-for-byte");

    free(image);
    puts("PASS Stage D1 common ESCape32 Boot Protocol integration");
    return 0;
}
