/* Host tests compile the real AM13E flash.c, not a duplicate algorithm. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "dl_flash.h"

uintptr_t boot_am13e_test_first;
uintptr_t boot_am13e_test_end;
extern int boot_am13e_flash_write(char *dst, const char *src, int len);

#define MAP_ADDRESS 0x10000000UL
#define MAP_LENGTH  0x00080000UL
#define APP_OFFSET  0x00006000UL
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while (0)

static uint8_t *flash_memory;
static unsigned erase_count;
static unsigned program_count;
static unsigned fail_next_erase;
static unsigned fail_next_program;
static unsigned fail_partial_program;

uint32_t DL_Flash_eraseSector(uint32_t addr) {
    ++erase_count;
    if (fail_next_erase) { --fail_next_erase; return DL_FLASH_ERROR; }
    if ((addr & (DL_FLASH_SECTOR_SIZE - 1U)) ||
        (uintptr_t)addr < boot_am13e_test_first ||
        (uintptr_t)addr > boot_am13e_test_end - DL_FLASH_SECTOR_SIZE)
        return DL_FLASH_ERROR;
    memset((void *)(uintptr_t)addr, 0xff, DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}

uint32_t DL_Flash_program(uint32_t addr, uint8_t *src, uint32_t len) {
    ++program_count;
    if (fail_next_program) { --fail_next_program; return DL_FLASH_ERROR; }
    if (fail_partial_program) {
        --fail_partial_program;
        if (len < 16U || (addr & 15U)) return DL_FLASH_ERROR;
        uint8_t *partial = (uint8_t *)(uintptr_t)addr;
        for (unsigned i = 0; i < 16U; ++i) {
            if ((uint8_t)(partial[i] & src[i]) != src[i]) return DL_FLASH_ERROR;
            partial[i] &= src[i];
        }
        return DL_FLASH_ERROR;
    }
    if (!src || !len || (addr & 15U) || (len & 15U) ||
        (uintptr_t)addr < boot_am13e_test_first ||
        (uintptr_t)addr > boot_am13e_test_end - len)
        return DL_FLASH_ERROR;
    uint8_t *dst = (uint8_t *)(uintptr_t)addr;
    for (unsigned i = 0; i < len; ++i)
        if ((uint8_t)(dst[i] & src[i]) != src[i])
            return DL_FLASH_ERROR;
    for (unsigned i = 0; i < len; ++i)
        dst[i] &= src[i];
    return DL_FLASH_SUCCESS;
}

static uint8_t payload[1024];
static uint8_t sig[1024];
static const uint8_t invalid[8] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
static unsigned tests;

static int write_block(unsigned block, const uint8_t *data, int size) {
    return boot_am13e_flash_write(
        (char *)(boot_am13e_test_first + 1024U * block),
        (const char *)data, size);
}

static void fill_blocks(void) {
    for (unsigned i = 0; i < sizeof payload; ++i) {
        payload[i] = (uint8_t)(i * 13U + 7U);
        sig[i] = (uint8_t)(i * 3U + 21U);
    }
    sig[0] = 0xea;
    sig[1] = 0x32;
}

static void check_signature_absent(void) {
    const uint8_t *head = (const uint8_t *)boot_am13e_test_first;
    CHECK(head[0] == 0xff && head[1] == 0xff);
}

static void test_invalidation_retry(void) {
    CHECK(write_block(0, invalid, 8) == 1);
    CHECK(write_block(0, invalid, 8) == 1);
    CHECK(write_block(1, invalid, 8) == 1);
    unsigned prior_erases = erase_count;
    CHECK(write_block(1, invalid, 8) == 1);
    CHECK(erase_count == prior_erases);
    check_signature_absent();
    puts("PASS invalidation retry");
    ++tests;
}

static void test_sequential_retry(void) {
    unsigned base = erase_count;
    CHECK(write_block(2, payload, 1024) == 1);
    CHECK(erase_count == base + 1);
    unsigned prior_prog = program_count;
    CHECK(write_block(2, payload, 1024) == 1);
    CHECK(program_count == prior_prog);
    CHECK(write_block(4, payload, 1024) == 0);
    CHECK(write_block(3, payload, 1024) == 1);
    CHECK(erase_count == base + 1);
    puts("PASS sequential, duplicate and skip");
    ++tests;
}

static void test_short_tail(void) {
    unsigned prior = program_count;
    CHECK(write_block(4, payload, 1004) == 1);
    CHECK(program_count > prior);
    const uint8_t *flash = (const uint8_t *)(boot_am13e_test_first + 4096);
    CHECK(memcmp(flash, payload, 1004) == 0);
    for (int i = 1004; i < 1008; ++i) CHECK(flash[i] == 0xff);
    puts("PASS short final block");
    ++tests;
}

static void test_restore_and_retry(void) {
    CHECK(write_block(0, sig, 1024) == 1);
    check_signature_absent();
    CHECK(write_block(0, sig, 1024) == 1);
    CHECK(write_block(1, payload, 1024) == 1);
    const uint8_t *head = (const uint8_t *)boot_am13e_test_first;
    CHECK(head[0] == 0xea && head[1] == 0x32);
    CHECK(write_block(0, sig, 1024) == 1);
    CHECK(write_block(1, payload, 1024) == 1);
    puts("PASS signature deferred and metadata retry");
    ++tests;
}

static void test_restart(void) {
    CHECK(write_block(0, invalid, 8) == 1);
    check_signature_absent();
    CHECK(write_block(1, invalid, 8) == 1);
    puts("PASS explicit restart invalidates signature");
    ++tests;
}

static void test_failed_program(void) {
    fail_next_program = 1;
    CHECK(write_block(2, payload, 1024) == 0);
    check_signature_absent();
    CHECK(write_block(2, payload, 1024) == 1);
    puts("PASS injected program error / retry");
    ++tests;
}

static void test_failed_erase(void) {
    /* A rejected erase must not advance the protocol transaction. */
    fail_next_erase = 1;
    CHECK(write_block(0, invalid, 8) == 0);
    CHECK(write_block(0, invalid, 8) == 1);
    CHECK(write_block(1, invalid, 8) == 1);
    check_signature_absent();
    puts("PASS injected erase error / restart");
    ++tests;
}

static void test_partial_program(void) {
    /* Program first 16 bytes, report failure, retry the same 1 KiB block. */
    unsigned prior_erases = erase_count;
    fail_partial_program = 1;
    CHECK(write_block(2, payload, 1024) == 0);
    CHECK(erase_count == prior_erases + 1);
    check_signature_absent();
    CHECK(write_block(2, payload, 1024) == 1);
    const uint8_t *flash = (const uint8_t *)(boot_am13e_test_first + 2048U);
    CHECK(memcmp(flash, payload, 1024U) == 0);
    check_signature_absent();
    puts("PASS injected partial-program failure / retry");
    ++tests;
}

int main(void) {
    void *region = mmap((void *)MAP_ADDRESS, MAP_LENGTH,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        return 2;
    }
    flash_memory = region;
    memset(flash_memory, 0xff, MAP_LENGTH);
    boot_am13e_test_first = MAP_ADDRESS + APP_OFFSET;
    boot_am13e_test_end = MAP_ADDRESS + MAP_LENGTH;
    fill_blocks();

    test_invalidation_retry();
    test_sequential_retry();
    test_short_tail();
    test_restore_and_retry();
    test_restart();
    test_failed_program();
    test_failed_erase();
    test_partial_program();
    printf("PASS %u host transaction tests\n", tests);
    return 0;
}
