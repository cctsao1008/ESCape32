/*
** Common ESCape32 service framing.
**
** The framing layer owns value complement checks and payload CRC behavior.
** Physical byte transport and CRC implementation are supplied by the target.
*/

#pragma once

#include <stdint.h>

typedef struct {
    int (*recv_buffer)(char *buffer, int length);
    void (*send_buffer)(const char *buffer, int length);
    uint32_t (*crc32)(const char *buffer, int length);
} boot_service_io_ops_t;

int boot_service_recv_value(const boot_service_io_ops_t *ops);
void boot_service_send_value(const boot_service_io_ops_t *ops, int value);
int boot_service_recv_data(
    const boot_service_io_ops_t *ops, char *buffer);
void boot_service_send_data(
    const boot_service_io_ops_t *ops, const char *buffer, int length);
