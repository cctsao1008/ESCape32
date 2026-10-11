/* ESCape32 Rel17 EXBUS input_mode=6, native frame decode only.
 * Production passes upstream crc16ccitt() directly as checksum callback.
 * Telemetry request is reported, but physical EXBUS reply TX and
 * original fallback 125/250k baud scheduling remain unimplemented.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint16_t (*AM13E_ExbusCrc16)(const char *,int);
bool am13e_exbus_decode_frame(const uint8_t *frame,unsigned len,
    unsigned ch1,unsigned ch2,AM13E_ExbusCrc16 crc,
    int *throttle,int *brake,bool *has_channels,bool *telemetry_request);
