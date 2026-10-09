#include "bidir_codec.h"
#include <stddef.h>

void am13e_bidir_encode_frame(uint16_t frame,
                              uint8_t levels[AM13E_BIDIR_DMA_LEVELS])
{
    static const uint8_t gcr[16] = {
        0x19, 0x1b, 0x12, 0x13, 0x1d, 0x15, 0x16, 0x17,
        0x1a, 0x09, 0x0a, 0x0b, 0x1e, 0x0d, 0x0e, 0x0f
    };
    if (levels == NULL) return;
    uint32_t encoded = 0U;
    for (unsigned i = 0U; i < 16U; i += 4U) {
        encoded |= (uint32_t)gcr[(frame >> i) & 0x0fU] << (i + i / 4U);
    }
    uint8_t level = 1U;
    levels[0] = 1U;
    for (unsigned bit = 0U; bit < AM13E_BIDIR_TELEMETRY_BITS; ++bit) {
        if (encoded & (UINT32_C(1) << (19U - bit))) level ^= 1U;
        levels[bit + 1U] = level;
    }
    levels[21] = 0U;
    levels[22] = 0U;
}
