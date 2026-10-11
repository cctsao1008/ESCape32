/* Rel17 SBUS/SBUS2 source-level receiver channels, not UART parity.
 * Hardware-specific framing, inversion and SBUS2 telemetry slots
 * require a separate qualified AM13E board UART provider.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool am13e_sbus_decode_channels(const uint8_t *frame,unsigned length,
                                unsigned throttle_channel,
                                unsigned brake_channel,
                                int *throttle_us,int *brake_us);
