#include "telem_frame_rx.h"
#include <stddef.h>
#include <string.h>
void am13e_telem_frame_init(AM13E_TelemFrameRx *s,unsigned mode)
{
    if(!s)return;
    memset(s,0,sizeof *s);
    s->mode=mode;
}
void am13e_telem_frame_abort(AM13E_TelemFrameRx *s)
{
    if(!s)return;
    s->used=0U;
    ++s->aborts;
}
/* Transport supplies the elapsed 16kHz ticks; never infer a
 * hardware UART IDLE event. An incomplete request must not survive
 * a frame gap, including a tick-counter wrap in the caller.
 */
void am13e_telem_frame_gap(AM13E_TelemFrameRx *s,
                           uint32_t elapsed_ticks,unsigned min_ticks)
{
    if(s&&min_ticks!=0U&&elapsed_ticks>=min_ticks&&s->used!=0U)
        am13e_telem_frame_abort(s);
}
bool am13e_telem_frame_push(AM13E_TelemFrameRx *s,uint8_t b,
                            uint8_t *out,unsigned capacity,unsigned *length)
{
    if(!s||!out||!length||s->mode>6U)return false;
    const unsigned want=s->mode==2U?4U:
                   (s->mode==3U||s->mode==6U)?2U:
                   s->mode==5U?1U:0U;
    if(want==0U||capacity<want)return false;
    if(s->used==0U&&
       ((s->mode==2U&&b!=4U)||
        (s->mode==3U&&b!=0x7eU)||
        (s->mode==6U&&b!=0x80U)))return false;
    if(s->used>=sizeof s->bytes){
        am13e_telem_frame_abort(s);
        return false;
    }
    /* A repeated S.Port/HoTT request delimiter is a new start,
     * not a response to the previous start byte. On the two-wire
     * transport this bounds recovery without inventing a PHY turn.
     */
    if(s->used==1U&&
       ((s->mode==3U&&b==0x7eU)||
        (s->mode==6U&&b==0x80U))){
        s->bytes[0]=b;
        return false;
    }
    s->bytes[s->used++]=b;
    if(s->used!=want)return false;
    memcpy(out,s->bytes,want);
    *length=want;
    s->used=0U;
    return true;
}
