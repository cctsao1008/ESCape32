/*
 * Portable CRC-32/ISO-HDLC implementation.
 */

#include "crc32.h"

#include <stddef.h>
#include <stdint.h>

uint32_t boot_crc32_begin(void)
{
    return 0xffffffffU;
}

uint32_t boot_crc32_update(
    uint32_t state, const void *data, uint32_t length)
{
    if ((data == NULL) && (length != 0U)) {
        return state;
    }

    const uint8_t *bytes = (const uint8_t *)data;

    for (uint32_t i = 0U; i < length; ++i) {
        state ^= bytes[i];

        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = 0U - (state & 1U);
            state = (state >> 1) ^ (0xedb88320U & mask);
        }
    }

    return state;
}

uint32_t boot_crc32_end(uint32_t state)
{
    return ~state;
}

uint32_t boot_crc32(const void *data, uint32_t length)
{
    return boot_crc32_end(
        boot_crc32_update(boot_crc32_begin(), data, length));
}
