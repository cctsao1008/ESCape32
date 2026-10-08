/* E62 host regression of common ESCape32 protocol and AM13E update ordering.
 * Run via tools/am13e/run-boot-host-tests.sh; does not access hardware. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "protocol.h"
#include "boot_update.h"

static int in[16], in_count, in_pos, out[16], out_count;
static int writes, reads;
static uint32_t read_offset, read_length, write_offset, write_length;
static bool allow_write = true;
static char memory[1024];
static jmp_buf done;

static int get_value(void) {
    if (in_pos < in_count) return in[in_pos++];
    longjmp(done, 1);
}
static void put_value(int v) { assert(out_count < 16); out[out_count++] = v; }
static int get_data(char *p) { memset(p, 0x5A, 1024); return 1024; }
static void put_data(const char *p, int n) {
    (void)p;
    assert(n == 32 || n == 20);
    put_value(n);
}
static uint32_t device_id(void) { return 0x12345678; }
static const char *read_app(uint32_t off, uint32_t len) {
    reads++; read_offset = off; read_length = len; return memory;
}
static bool write_app(uint32_t off, const char *p, uint32_t len) {
    (void)p;
    writes++; write_offset = off; write_length = len; return allow_write;
}
static bool app_valid(void) { return false; }
static void jump_app(void) { assert(!"unexpected APP jump"); }

static void run_protocol(const int *values, int n) {
    in_count = n; in_pos = out_count = writes = reads = 0;
    allow_write = true;
    for (int i = 0; i < n; i++) in[i] = values[i];
    boot_protocol_ops_t ops = {
        .io_id = 7,
        .recv_value = get_value, .send_value = put_value,
        .recv_data = get_data, .send_data = put_data,
        .device_id = device_id, .map_read = read_app,
        .write_block = write_app, .app_valid = app_valid,
        .jump_app = jump_app
    };
    if (setjmp(done) == 0) boot_protocol_run(&ops);
}

/* The AM13E update module calls this port abstraction; never real Flash. */
bool boot_port_write_block(uint32_t off, const char *p, uint32_t len) {
    return write_app(off, p, len);
}

static void test_protocol(void) {
    const int probe[] = {BOOT_CMD_PROBE};
    run_protocol(probe, 1);
    assert(out_count == 1 && out[0] == BOOT_RES_OK);

    const int info[] = {BOOT_CMD_INFO};
    run_protocol(info, 1);
    assert(out_count == 1 && out[0] == 32);

    const int read[] = {BOOT_CMD_READ, 2, 4};
    run_protocol(read, 3);
    assert(reads == 1 && read_offset == 2048 && read_length == 20);
    assert(out_count == 1 && out[0] == 20);

    const int write[] = {BOOT_CMD_WRITE, 1};
    run_protocol(write, 2);
    assert(writes == 1 && write_offset == 1024 && write_length == 1024);
    assert(out_count == 1 && out[0] == BOOT_RES_OK);

    puts("[PASS] PROBE / INFO / READ / WRITE command dispatch");
}

static void test_update(void) {
    char block[1024] = {0};
    allow_write = true;
    boot_update_reset_session();
    assert(!boot_update_session_active());
    assert(!boot_update_write_block(1024, block, 1024));
    assert(boot_update_write_block(0, block, 1024));
    assert(boot_update_session_active() && boot_update_next_offset() == 1024);
    assert(!boot_update_write_block(2048, block, 1024));
    assert(boot_update_write_block(1024, block, 1024));
    assert(boot_update_next_offset() == 2048);
    assert(boot_update_write_block(0, block, 1024));
    assert(boot_update_next_offset() == 1024);
    allow_write = false;
    assert(!boot_update_write_block(1024, block, 1024));
    assert(boot_update_next_offset() == 1024);
    allow_write = true;
    assert(boot_update_write_block(1024, block, 1024));
    assert(boot_update_next_offset() == 2048);
    assert(!boot_update_write_block(2048, block, 3));
    assert(!boot_update_write_block(2048, block, 1028));
    puts("[PASS] sequential update / restart / failure / length guard");
}
int main(void) {
    test_protocol();
    test_update();
    puts("[PASS] E62 Boot host regression");
    return 0;
}
