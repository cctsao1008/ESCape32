/* Real ESCape32 service_io.c + boot CRC32 host regression. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "service_io.h"
#include "crc32.h"

static unsigned char rx[1100], tx[1100];
static size_t rx_size, rx_pos, tx_size;
static int receive(char *dst, int n) {
    if (n < 0 || (size_t)n > rx_size - rx_pos) return 0;
    memcpy(dst, rx + rx_pos, (size_t)n);
    rx_pos += (size_t)n;
    return 1;
}
static void send(const char *src, int n) {
    assert(n >= 0 && (size_t)n <= sizeof(tx) - tx_size);
    memcpy(tx + tx_size, src, (size_t)n);
    tx_size += (size_t)n;
}
static uint32_t crc(const char *p, int n) {
    assert(n >= 0);
    return boot_crc32(p, (uint32_t)n);
}
static const boot_service_io_ops_t ops = {
    .recv_buffer=receive, .send_buffer=send, .crc32=crc
};
static void setup(const unsigned char *p, size_t n) {
    assert(n <= sizeof(rx));
    memcpy(rx, p, n);
    rx_pos = 0; rx_size = n; tx_size = 0;
}
int main(void) {
    unsigned char good[] = {0x12, 0xED};
    setup(good, sizeof good);
    assert(boot_service_recv_value(&ops) == 0x12);
    unsigned char bad[] = {0x12, 0x12};
    setup(bad, sizeof bad);
    assert(boot_service_recv_value(&ops) == -1);
    setup(good, 1);
    assert(boot_service_recv_value(&ops) == -1);
    tx_size = 0;
    boot_service_send_value(&ops, 0xA5);
    assert(tx_size == 2 && tx[0] == 0xA5 && tx[1] == 0x5A);
    puts("[PASS] complemented value / malformed / truncated framing");

    const char data[] = "ABCD1234";
    tx_size = 0;
    boot_service_send_data(&ops, data, 8);
    assert(tx_size == 14);
    assert(tx[0] == 1 && tx[1] == 0xFE);
    unsigned char packet[14];
    memcpy(packet, tx, sizeof packet);
    setup(packet, sizeof packet);
    char output[1024] = {0};
    assert(boot_service_recv_data(&ops, output) == 8);
    assert(memcmp(output, data, 8) == 0);
    puts("[PASS] 8-byte framed round-trip with production CRC");

    packet[4] ^= 1;
    setup(packet, sizeof packet);
    assert(boot_service_recv_data(&ops, output) == -1);
    packet[4] ^= 1;
    packet[13] ^= 1;
    setup(packet, sizeof packet);
    assert(boot_service_recv_data(&ops, output) == -1);
    packet[13] ^= 1;
    setup(packet, sizeof packet - 1);
    assert(boot_service_recv_data(&ops, output) == -1);
    packet[1] ^= 1;
    setup(packet, sizeof packet);
    assert(boot_service_recv_data(&ops, output) == -1);
    puts("[PASS] payload / CRC / truncation / length-prefix corruption rejected");
    puts("[PASS] production service_io.c host regression");
    return 0;
}
