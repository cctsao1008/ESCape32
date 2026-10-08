/*
** Common ESCape32 service framing.
*/

#include "service_io.h"

#include <stddef.h>

static int boot_service_ops_valid(const boot_service_io_ops_t *ops)
{
    return (ops != NULL) &&
        (ops->recv_buffer != NULL) &&
        (ops->send_buffer != NULL) &&
        (ops->crc32 != NULL);
}

int boot_service_recv_value(const boot_service_io_ops_t *ops)
{
    char buffer[2];

    if (!boot_service_ops_valid(ops)) {
        return -1;
    }

    return ops->recv_buffer(buffer, 2) &&
        ((buffer[0] ^ buffer[1]) == (char)0xff)
        ? (unsigned char)buffer[0]
        : -1;
}

void boot_service_send_value(const boot_service_io_ops_t *ops, int value)
{
    if (!boot_service_ops_valid(ops)) {
        return;
    }

    char buffer[2] = {
        (char)value,
        (char)~value,
    };

    ops->send_buffer(buffer, 2);
}

int boot_service_recv_data(
    const boot_service_io_ops_t *ops, char *buffer)
{
    if (!boot_service_ops_valid(ops) || (buffer == NULL)) {
        return -1;
    }

    int count = boot_service_recv_value(ops);
    if (count == -1) {
        return -1;
    }

    int length = (count + 1) << 2;
    uint32_t expected_crc;

    return ops->recv_buffer(buffer, length) &&
        ops->recv_buffer((char *)&expected_crc, 4) &&
        (ops->crc32(buffer, length) == expected_crc)
        ? length
        : -1;
}

void boot_service_send_data(
    const boot_service_io_ops_t *ops, const char *buffer, int length)
{
    if (!boot_service_ops_valid(ops) ||
        (buffer == NULL) ||
        (length <= 0) ||
        ((length & 0x3) != 0)) {
        return;
    }

    uint32_t crc = ops->crc32(buffer, length);

    boot_service_send_value(ops, (length >> 2) - 1);
    ops->send_buffer(buffer, length);
    ops->send_buffer((const char *)&crc, 4);
}
