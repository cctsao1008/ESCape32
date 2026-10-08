/*
 * AM13E23019 boot platform adapter.
 *
 * Common ESCape32 boot semantics must not depend on AM13E registers, Flash
 * details, or the fixed application address. Those target details terminate
 * here and in the lower-level boot_flash backend.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

bool boot_port_app_valid(void);
void boot_port_jump_to_app(void);

const char *boot_port_map_read(uint32_t offset, uint32_t length);
bool boot_port_write_block(
    uint32_t offset, const char *data, uint32_t length);
