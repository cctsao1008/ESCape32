/* ESCape32 Rel17 original iBUS input_mode=3, transport independent. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool am13e_ibus_decode_channels(const uint8_t *packet,unsigned length,
                                unsigned throttle_channel,
                                unsigned brake_channel,
                                int *throttle_us,int *brake_us);
