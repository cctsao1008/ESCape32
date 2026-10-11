/* ESCape32 Rel17-compatible Bidirectional DShot telemetry line encoding.
 * Physical turnaround/waveform TX is handled by the AM13E PB14 backend.
 */
#pragma once
#include <stdint.h>
#define AM13E_BIDIR_TELEMETRY_BITS 20U
#define AM13E_BIDIR_DMA_LEVELS 23U
/* Input is original Rel17 12-bit payload + INVERTED DShot CRC nibble.
 * levels[0] is initial HIGH, levels[1..20] are 20 NRZI/GCR levels,
 * and [21..22] are LOW end-of-reply levels. Each entry is 0 or 1,
 * not the final electrical polarity/drive strength.
 */
void am13e_bidir_encode_frame(uint16_t frame,
                              uint8_t levels[AM13E_BIDIR_DMA_LEVELS]);

/* Single Core-owned GCR packing used by both legacy and native serializers. */
static inline uint32_t rel17_bidir_gcr(int frame) {
    static const char gcr[16] = {
        0x19, 0x1b, 0x12, 0x13, 0x1d, 0x15, 0x16, 0x17,
        0x1a, 0x09, 0x0a, 0x0b, 0x1e, 0x0d, 0x0e, 0x0f
    };
    int encoded=0;
    for(int i=0,j=0;i<16;i+=4,j+=5)encoded|=gcr[frame>>i&0xf]<<j;
    return encoded;
}
