/* Native UART FIFO byte accumulator, no invented packet CRC.
 * Original iBUS uses 0x20,0x40 and 32 bytes; SBUS uses 0x0f and 25.
 * On UART parity/noise/frame/overrun errors, caller must abort RX.
 */
#include "serial_frame_rx.h"
#include <string.h>
#include <stddef.h>
void am13e_serial_frame_init(AM13E_SerialFrameRx *rx,unsigned mode)
{
    if(!rx)return;
    memset(rx,0,sizeof *rx);
    rx->mode=mode;
}
void am13e_serial_frame_abort(AM13E_SerialFrameRx *rx)
{
    if(!rx)return;
    rx->used=0U;
    memset(rx->bytes,0,sizeof rx->bytes);
    ++rx->frame_errors;
}
bool am13e_serial_frame_byte(AM13E_SerialFrameRx *rx,uint8_t b,
                             uint8_t ready[32],unsigned *len)
{
    if(!rx||!ready||!len||(rx->mode!=3U&&rx->mode!=4U))return false;
    const uint8_t start=rx->mode==3U?0x20U:0x0fU;
    const unsigned total=rx->mode==3U?32U:25U;
    if(rx->used==0U){
        if(b!=start)return false;
    } else if(rx->mode==3U&&rx->used==1U&&b!=0x40U){
        /* Preserve potential 0x20 as the next header when a partial
         * iBUS start was followed by another packet's first byte.
         */
        rx->used=0U;
        if(b!=start)return false;
    }
    rx->bytes[rx->used++]=b;
    if(rx->used!=total)return false;
    memcpy(ready,rx->bytes,total);
    *len=total;
    rx->used=0U;
    return true;
}
