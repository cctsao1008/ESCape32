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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_PROTOCOL_REVISION   4U
#define BOOT_PROTOCOL_BLOCK_SIZE 1024U

#define BOOT_CMD_PROBE  0
#define BOOT_CMD_INFO   1
#define BOOT_CMD_READ   2
#define BOOT_CMD_WRITE  3
#define BOOT_CMD_UPDATE 4
#define BOOT_CMD_SETWRP 5

#define BOOT_RES_OK    0
#define BOOT_RES_ERROR 1

typedef struct {
    uint8_t io_id;

    int (*recv_value)(void);
    void (*send_value)(int value);
    int (*recv_data)(char *buffer);
    void (*send_data)(const char *buffer, int length);

    uint32_t (*device_id)(void);

    const char *(*map_read)(uint32_t offset, uint32_t length);
    bool (*write_block)(uint32_t offset, const char *data, uint32_t length);

    /*
     * UPDATE and SETWRP are optional platform/manufacturing extensions.
     * If supplied, the callback owns the command-specific sub-protocol.
     */
    void (*handle_update)(void);
    void (*handle_setwrp)(void);

    bool (*app_valid)(void);
    void (*jump_app)(void);
} boot_protocol_ops_t;

void boot_protocol_run(const boot_protocol_ops_t *ops);
