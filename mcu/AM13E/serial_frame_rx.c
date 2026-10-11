/* Original transport framing: iBUS 0x20/0x40 32B,
 * SBUS 0x0f 25B, CRSF length+2 4..64B. CRC belongs to
 * protocol receiver; do not invent a CRSF address constraint.
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
    rx->expected=0U;
    memset(rx->bytes,0,sizeof rx->bytes);
    ++rx->frame_errors;
}
bool am13e_serial_frame_byte(AM13E_SerialFrameRx *rx,uint8_t b,
                             uint8_t *out,unsigned capacity,
                             unsigned *length)
{
    if(!rx||!out||!length||capacity==0U||
       (rx->mode!=3U&&rx->mode!=4U&&rx->mode!=5U&&
        rx->mode!=6U&&rx->mode!=7U))
        return false;
    if(rx->used==0U){
        if((rx->mode==3U&&b!=0x20U)||
           (rx->mode==4U&&b!=0x0fU)||
           (rx->mode==7U&&b!=0xa8U))return false;
        rx->expected=rx->mode==3U?32U:
                     rx->mode==4U?25U:0U;
    } else if(rx->mode==3U&&rx->used==1U&&b!=0x40U){
        rx->used=0U;
        rx->expected=32U;
        if(b!=0x20U)return false;
    } else if(rx->mode==7U&&rx->used==2U){
        /* Original SUMD length = 5 + channel_count*2. */
        if(b>29U){
            am13e_serial_frame_abort(rx);
            return false;
        }
        rx->expected=5U+((unsigned)b<<1U);
    } else if(rx->mode==6U&&rx->used==2U){
        /* EXBUS length byte at offset 2. The original parser does
         * not require a fixed start address: idle-framed packets
         * must be synchronized by the board transport.
         */
        if(b<8U||b>AM13E_SERIAL_MAX_FRAME){
            am13e_serial_frame_abort(rx);
            return false;
        }
        rx->expected=b;
    } else if(rx->mode==5U&&rx->used==1U){
        if(b<2U||b>62U){
            am13e_serial_frame_abort(rx);
            return false;
        }
        rx->expected=(unsigned)b+2U;
    }
    if(rx->mode==6U && rx->used==5U &&
       (unsigned)b+8U!=rx->expected){
        am13e_serial_frame_abort(rx);
        return false;
    }
    if(rx->used>=AM13E_SERIAL_MAX_FRAME){
        am13e_serial_frame_abort(rx);
        return false;
    }
    rx->bytes[rx->used++]=b;
    if(rx->expected==0U||rx->used<rx->expected)return false;
    if(rx->used!=rx->expected||capacity<rx->expected){
        am13e_serial_frame_abort(rx);
        return false;
    }
    memcpy(out,rx->bytes,rx->expected);
    *length=rx->expected;
    rx->used=0U;
    rx->expected=0U;
    return true;
}
