/* E62 Boot recovery protocol regression, using the real protocol.c.
 * Stop the intentionally infinite command loop via a test-only longjmp.
 */
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

static jmp_buf stop;
static int calls, checks, jumps, probes, written;
static bool valid;
static int input[8], size, index;
static int recv_value(void) {
    if (++calls > 8) longjmp(stop, 1);
    if (index < size) return input[index++];
    return -1; /* PB14 idle/timeout */
}
static void send_value(int v) { if (v == BOOT_RES_OK) probes++; }
static int recv_data(char *p) { memset(p, 0xA5, BOOT_PROTOCOL_BLOCK_SIZE); return 1024; }
static void send_data(const char *p, int n) { (void)p; (void)n; }
static uint32_t device_id(void) { return 0; }
static const char *map_read(uint32_t p, uint32_t n) { (void)p; (void)n; return NULL; }
static bool write_block(uint32_t p, const char *d, uint32_t n) {
    (void)p; (void)d; (void)n; written++; return true;
}
static bool app_valid(void) { checks++; return valid; }
static void jump_app(void) { jumps++; longjmp(stop, 2); }
static void run(const int *values, int count, bool is_valid) {
    calls = checks = jumps = probes = written = index = 0;
    size = count; valid = is_valid;
    for (int i = 0; i < count; i++) input[i] = values[i];
    const boot_protocol_ops_t ops = {
        .recv_value=recv_value, .send_value=send_value,
        .recv_data=recv_data, .send_data=send_data,
        .device_id=device_id, .map_read=map_read,
        .write_block=write_block, .app_valid=app_valid,
        .jump_app=jump_app,
    };
    int why = setjmp(stop);
    if (why == 0) boot_protocol_run(&ops);
    if (is_valid) assert(why == 2);
    else assert(why == 1);
}
int main(void) {
    run(NULL, 0, false);
    assert(checks >= 3 && jumps == 0);
    puts("[PASS] invalid APP: repeated idle timeouts remain in service loop");

    const int probe[] = {BOOT_CMD_PROBE};
    run(probe, 1, false);
    assert(probes == 1 && checks >= 2 && jumps == 0);
    puts("[PASS] invalid APP: timeout recovery accepts late PROBE");

    const int write[] = {BOOT_CMD_WRITE, 0};
    run(write, 2, false);
    assert(written == 1 && jumps == 0);
    puts("[PASS] invalid APP: accepts block-0 programming in recovery");

    run(NULL, 0, true);
    assert(checks == 1 && jumps == 1);
    puts("[PASS] valid APP: timeout requests APP handoff");
    puts("[PASS] E62 Boot recovery protocol host regression");
    return 0;
}
