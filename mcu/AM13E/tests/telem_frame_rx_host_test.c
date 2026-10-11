#include "telem_frame_rx.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AM13E_TelemFrameRx s;
    uint8_t out[4]={0};unsigned n=0U;
    am13e_telem_frame_init(&s,2U);
    assert(!am13e_telem_frame_push(&s,0xffU,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,4U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x81U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x20U,out,4U,&n));
    assert(am13e_telem_frame_push(&s,0x31U,out,4U,&n));
    assert(n==4U&&out[0]==4U&&out[1]==0x81U&&out[3]==0x31U);
    assert(!am13e_telem_frame_push(&s,4U,out,1U,&n));
    assert(s.used==0U);
    assert(!am13e_telem_frame_push(&s,4U,out,4U,&n));
    am13e_telem_frame_abort(&s);
    assert(s.used==0U&&s.aborts==1U);
    am13e_telem_frame_init(&s,3U);
    assert(!am13e_telem_frame_push(&s,0U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    assert(am13e_telem_frame_push(&s,0x10U,out,4U,&n));
    assert(n==2U&&out[0]==0x7eU);
    am13e_telem_frame_init(&s,5U);
    assert(am13e_telem_frame_push(&s,0x5aU,out,4U,&n));
    assert(n==1U&&out[0]==0x5aU);
    am13e_telem_frame_init(&s,6U);
    assert(!am13e_telem_frame_push(&s,0U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x80U,out,4U,&n));
    assert(am13e_telem_frame_push(&s,0x8cU,out,4U,&n));
    assert(n==2U&&out[0]==0x80U&&out[1]==0x8cU);
    for(unsigned mode=0U;mode<=6U;++mode){
        if(mode==0U||mode==1U||mode==4U){
            am13e_telem_frame_init(&s,mode);
            assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
            assert(s.used==0U);
        }
    }
    puts("PASS Rel17 telemetry iBUS/S.Port/MSB/HoTT request framing, RX abort and bounds");
    return 0;
}
