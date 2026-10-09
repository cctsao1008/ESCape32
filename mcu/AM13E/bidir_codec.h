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
