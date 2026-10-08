/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** Common ESCape32 boot command semantics.
*/

#include "protocol.h"

#include <stddef.h>

static bool boot_protocol_ops_valid(const boot_protocol_ops_t *ops)
{
    return (ops != NULL) &&
        (ops->recv_value != NULL) &&
        (ops->send_value != NULL) &&
        (ops->recv_data != NULL) &&
        (ops->send_data != NULL) &&
        (ops->device_id != NULL) &&
        (ops->map_read != NULL) &&
        (ops->write_block != NULL) &&
        (ops->app_valid != NULL) &&
        (ops->jump_app != NULL);
}

void boot_protocol_run(const boot_protocol_ops_t *ops)
{
    char write_buffer[BOOT_PROTOCOL_BLOCK_SIZE];

    if (!boot_protocol_ops_valid(ops)) {
        return;
    }

    for (;;) {
        switch (ops->recv_value()) {
        case BOOT_CMD_PROBE:
            ops->send_value(BOOT_RES_OK);
            break;

        case BOOT_CMD_INFO: {
            uint32_t device = ops->device_id();
            char info[32] = {
                (char)BOOT_PROTOCOL_REVISION,
                (char)ops->io_id,
                (char)device,
                (char)(device >> 8),
                (char)(device >> 16),
                (char)(device >> 24),
            };
            ops->send_data(info, sizeof(info));
            break;
        }

        case BOOT_CMD_READ: {
            int block = ops->recv_value();
            if (block == -1) {
                goto try_app;
            }

            int count = ops->recv_value();
            if (count == -1) {
                goto try_app;
            }

            uint32_t length = ((uint32_t)count + 1U) << 2;
            uint32_t offset = (uint32_t)block << 10;
            const char *source = ops->map_read(offset, length);

            if (source == NULL) {
                ops->send_value(BOOT_RES_ERROR);
                break;
            }

            ops->send_data(source, (int)length);
            break;
        }

        case BOOT_CMD_WRITE: {
            int block = ops->recv_value();
            if (block == -1) {
                goto try_app;
            }

            int length = ops->recv_data(write_buffer);
            if (length == -1) {
                goto try_app;
            }

            uint32_t offset = (uint32_t)block << 10;
            bool ok = ops->write_block(
                offset, write_buffer, (uint32_t)length);
            ops->send_value(ok ? BOOT_RES_OK : BOOT_RES_ERROR);
            break;
        }

        case BOOT_CMD_UPDATE:
            if (ops->handle_update != NULL) {
                if (ops->handle_update()) {
                    goto try_app;
                }
            } else {
                ops->send_value(BOOT_RES_ERROR);
            }
            break;

        case BOOT_CMD_SETWRP:
            if (ops->handle_setwrp != NULL) {
                ops->handle_setwrp();
            } else {
                ops->send_value(BOOT_RES_ERROR);
            }
            break;

        default:
try_app:
            if (ops->app_valid()) {
                ops->jump_app();
            }
            break;
        }
    }
}
