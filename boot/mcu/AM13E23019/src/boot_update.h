/*
 * E62 AM13E application update transaction.
 *
 * The product has one contiguous APP region. This layer adds transaction
 * ordering on top of the ESCape32 WRITE command so an update starts at APP
 * offset 0 and proceeds monotonically through the packed image.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

bool boot_update_write_block(
    uint32_t offset, const char *data, uint32_t length);

void boot_update_reset_session(void);
bool boot_update_session_active(void);
uint32_t boot_update_next_offset(void);
