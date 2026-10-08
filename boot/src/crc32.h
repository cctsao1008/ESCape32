/*
 * Small portable CRC-32/ISO-HDLC helper used by the AM13 Boot image/service
 * paths.
 */
#pragma once

#include <stdint.h>

uint32_t boot_crc32_begin(void);
uint32_t boot_crc32_update(
    uint32_t state, const void *data, uint32_t length);
uint32_t boot_crc32_end(uint32_t state);
uint32_t boot_crc32(const void *data, uint32_t length);
