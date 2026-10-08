/*
 * E62 update fault-injection host regression.
 * Uses the production boot_update.c state machine, with an injectable Flash
 * adapter. Simulated erase/program interruption is NOT a NVMNW electrical
 * model and does not establish on-target power-loss safety.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "boot_update.h"

enum { BLOCK = 1024, SECTOR = 2048, BLOCKS = 6 };
static unsigned char flash[BLOCK * BLOCKS];
static unsigned char payload[BLOCK];
static unsigned write_calls;
static int fail_write_on;
static bool cut_during_write;
static bool reset_observed;

static void reset_mock(void)
{
    memset(flash, 0xFF, sizeof flash);
    memset(payload, 0xA5, sizeof payload);
    write_calls = 0;
    fail_write_on = -1;
    cut_during_write = false;
    reset_observed = false;
    boot_update_reset_session();
}

/* Mock faithfully captures the 2 KiB sector erase cadence, not NVMNW ECC. */
bool boot_port_write_block(uint32_t offset, const char *data, uint32_t length)
{
    assert(offset <= sizeof flash && length <= sizeof flash - offset);
    ++write_calls;
    if (fail_write_on > 0 && write_calls == (unsigned)fail_write_on)
        return false;

    if (offset % SECTOR == 0)
        memset(flash + offset, 0xFF, SECTOR);
    if (cut_during_write) {
        /* Model loss of power after partial programming of first page. */
        memcpy(flash + offset, data, 16);
        reset_observed = true;
        return false;
    }
    memcpy(flash + offset, data, length);
    return true;
}

static void test_reboot_clears_session(void)
{
    reset_mock();
    assert(boot_update_write_block(0, (char *)payload, BLOCK));
    assert(boot_update_next_offset() == BLOCK);
    boot_update_reset_session(); /* process reboot */
    assert(!boot_update_session_active());
    assert(boot_update_next_offset() == 0);
    assert(!boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    assert(boot_update_write_block(0, (char *)payload, BLOCK));
    puts("[PASS] reboot clears transaction offset; block 0 restart required");
}

static void test_flash_failure_no_advance(void)
{
    reset_mock();
    fail_write_on = 1;
    assert(!boot_update_write_block(0, (char *)payload, BLOCK));
    assert(boot_update_next_offset() == 0);
    fail_write_on = -1;
    assert(boot_update_write_block(0, (char *)payload, BLOCK));
    fail_write_on = 3;
    assert(!boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    assert(boot_update_next_offset() == BLOCK);
    fail_write_on = -1;
    assert(boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    puts("[PASS] injected Flash failure does not advance update cursor");
}

static void test_partial_program_recovery(void)
{
    reset_mock();
    cut_during_write = true;
    assert(!boot_update_write_block(0, (char *)payload, BLOCK));
    assert(reset_observed);
    assert(boot_update_next_offset() == 0);
    boot_update_reset_session();
    cut_during_write = false;
    assert(!boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    assert(boot_update_write_block(0, (char *)payload, BLOCK));
    assert(boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    puts("[PASS] partial block failure followed by reboot/restart recovers");
}

static void test_out_of_order(void)
{
    reset_mock();
    assert(!boot_update_write_block(BLOCK, (char *)payload, BLOCK));
    assert(boot_update_write_block(0, (char *)payload, BLOCK));
    assert(!boot_update_write_block(3 * BLOCK, (char *)payload, BLOCK));
    assert(!boot_update_write_block(0, NULL, BLOCK));
    assert(!boot_update_write_block(BLOCK, (char *)payload, 0));
    assert(!boot_update_write_block(BLOCK, (char *)payload, BLOCK - 1));
    assert(boot_update_next_offset() == BLOCK);
    puts("[PASS] out-of-order, null, zero and unaligned writes rejected");
}

int main(void)
{
    test_reboot_clears_session();
    test_flash_failure_no_advance();
    test_partial_program_recovery();
    test_out_of_order();
    puts("[PASS] E62 Boot update fault-injection host regression");
    return 0;
}
