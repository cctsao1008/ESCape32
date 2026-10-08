/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** Legacy STM32/AT32 adapter for the common ESCape32 boot command engine.
*/

#include "common.h"
#include "protocol.h"

#include <stdbool.h>
#include <stdint.h>

#define REVISION 4
_Static_assert(REVISION == BOOT_PROTOCOL_REVISION,
    "legacy boot revision must match common protocol revision");

static uint32_t legacy_device_id(void)
{
    return DBGMCU_IDCODE;
}

static const char *legacy_map_read(uint32_t offset, uint32_t length)
{
    (void)length;
    return _rom_end + offset;
}

static bool legacy_write_block(
    uint32_t offset, const char *data, uint32_t length)
{
    return write(
        _rom_end + offset, data, (int)length) != 0;
}

static bool legacy_app_valid(void)
{
    return *(uint16_t *)_rom_end == 0x32ea;
}

__attribute__((noreturn))
static void legacy_jump_app(void)
{
    const uint32_t *vector =
        (const uint32_t *)(_rom_end + PAGE_SIZE);

    __asm__ volatile (
        "msr msp, %0\n\t"
        "bx %1\n\t"
        :: "r" (vector[0]), "r" (vector[1]) : "memory");

    __builtin_unreachable();
}

static bool legacy_handle_update(void)
{
    char *buffer = _ram_end;
    int position = 0;

    for (int i = 0, blocks = (_rom_end - _rom) >> 10;
         i < blocks;
         ++i) {
        int length = recvdata(buffer + position);
        if (length == -1) {
            return true;
        }

        sendval(BOOT_RES_OK);
        position += length;

        if (length < (int)BOOT_PROTOCOL_BLOCK_SIZE) {
            break;
        }
    }

    update(_rom, buffer, position);

    /*
     * The legacy update() path resets on success and does not return.
     * Returning here therefore represents an update failure.
     */
    sendval(BOOT_RES_ERROR);
    return false;
}

static void legacy_handle_setwrp(void)
{
    switch (recvval()) {
    case 0x33:
        setwrp(0);
        break;
    case 0x44:
        setwrp(1);
        break;
    case 0x55:
        setwrp(2);
        break;
    default:
        break;
    }

    /*
     * The legacy setwrp() implementation resets when the option update
     * succeeds. A returned call therefore preserves the upstream error reply.
     */
    sendval(BOOT_RES_ERROR);
}

static const boot_protocol_ops_t legacy_boot_ops = {
    .io_id = IO_PIN,
    .recv_value = recvval,
    .send_value = sendval,
    .recv_data = recvdata,
    .send_data = senddata,
    .device_id = legacy_device_id,
    .map_read = legacy_map_read,
    .write_block = legacy_write_block,
    .handle_update = legacy_handle_update,
    .handle_setwrp = legacy_handle_setwrp,
    .app_valid = legacy_app_valid,
    .jump_app = legacy_jump_app,
};

void main(void)
{
    init();
    initio();

    if (RCC_CSR & (RCC_CSR_SFTRSTF | RCC_CSR_OBLRSTF)) {
        RCC_CSR = RCC_CSR_RMVF;
        sendval(BOOT_RES_OK);
    }
#ifdef FAST_EXIT
    else if (legacy_app_valid()) {
        legacy_jump_app();
    }
#endif

    boot_protocol_run(&legacy_boot_ops);

    for (;;) {
    }
}
