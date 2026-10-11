#include "../bidir_codec.h"
#include <stddef.h>

void am13e_bidir_encode_frame(uint16_t frame,
                              uint8_t levels[AM13E_BIDIR_DMA_LEVELS])
{
    if(levels==NULL)return;
    const uint32_t encoded=rel17_bidir_gcr(frame);
    uint8_t level = 1U;
    levels[0] = 1U;
    for (unsigned bit = 0U; bit < AM13E_BIDIR_TELEMETRY_BITS; ++bit) {
        if (encoded & (UINT32_C(1) << (19U - bit))) level ^= 1U;
        levels[bit + 1U] = level;
    }
    levels[21] = 0U;
    levels[22] = 0U;
}
