/* Rel17 HoTT/SUMD input_mode=7; original crc16xmodem()
 * callback is supplied by src/io.c (original src/util.c symbol).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef uint16_t (*AM13E_HottCrc16)(const char *,int);
bool am13e_hott_decode_frame(const uint8_t *frame,unsigned length,
    unsigned throttle_channel,unsigned brake_channel,AM13E_HottCrc16 crc,
    int *throttle_us,int *brake_us,bool *has_channels);
