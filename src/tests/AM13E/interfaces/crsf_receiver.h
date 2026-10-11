/* Rel17 CRSF input_mode=5 channel receive only. UART TX command
 * replies (0x28/0x2c/0x2d) require a separate native provider.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool am13e_crsf_decode_frame(const uint8_t *frame,unsigned length,
                             unsigned throttle_channel,
                             unsigned brake_channel,
                             int *throttle_us,int *brake_us,
                             bool *has_channels);
