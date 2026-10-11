/* Original Rel17 USART1 request framing, independent of physical UART.
 * The original src/telem.c protocol function validates response policy.
 * KISS/CRSF TX-only; iBUS=4B, S.Port=2B, MSB=1B, HoTT=2B.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t bytes[4];
    unsigned used,mode;
    uint32_t aborts;
} AM13E_TelemFrameRx;
void am13e_telem_frame_init(AM13E_TelemFrameRx *,unsigned mode);
void am13e_telem_frame_abort(AM13E_TelemFrameRx *);
bool am13e_telem_frame_push(AM13E_TelemFrameRx *,uint8_t b,
                            uint8_t *out,unsigned capacity,unsigned *len);
