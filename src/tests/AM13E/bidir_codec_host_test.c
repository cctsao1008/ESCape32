/* Exact, exhaustive waveform parity against ESCape32 Rel17
 * src/io.c::iotim_dma_isr() GCR/NRZI serializer, all 4096 payloads.
 */
#include "bidir_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static uint16_t frame_from_payload(unsigned telemetry)
{
    unsigned v = (telemetry & 0x0fffU) << 4U;
    unsigned crc = 0U;
    for (unsigned part = v; part; part >>= 4U) crc ^= part;
    return (uint16_t)(v | ((crc ^ 0x0fU) & 0x0fU));
}
static void rel17_reference(uint16_t frame, uint8_t output[23])
{
    static const char gcr[] = {
        0x19, 0x1b, 0x12, 0x13, 0x1d, 0x15, 0x16, 0x17,
        0x1a, 0x09, 0x0a, 0x0b, 0x1e, 0x0d, 0x0e, 0x0f
    };
    int encoded = 0;
    for (int i = 0, j = 0; i < 16; i += 4, j += 5)
        encoded |= gcr[(frame >> i) & 0xf] << j;
    output[0] = 1;
    for (int p = -1, i = 19; i >= 0; --i) {
        if ((encoded >> i) & 1) p = ~p;
        output[20 - i] = (p != 0);
    }
    output[21] = 0;
    output[22] = 0;
}
int main(void)
{
    uint8_t actual[AM13E_BIDIR_DMA_LEVELS], expected[23];
    for (unsigned telemetry = 0U; telemetry < 4096U; ++telemetry) {
        const uint16_t frame = frame_from_payload(telemetry);
        am13e_bidir_encode_frame(frame, actual);
        rel17_reference(frame, expected);
        for (unsigned i = 0; i < 23U; ++i) assert(actual[i] == expected[i]);
        assert(actual[0] == 1U && actual[21] == 0U && actual[22] == 0U);
    }
    puts("AM13E BiDShot GCR/NRZI 4096 payloads vs Rel17 PASS");
    return 0;
}
