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
    /* Back-to-back start delimiters must not dispatch stale requests. */
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    assert(s.used==1U);
    assert(am13e_telem_frame_push(&s,0x1bU,out,4U,&n));
    assert(n==2U&&out[0]==0x7eU&&out[1]==0x1bU);
    am13e_telem_frame_init(&s,5U);
    assert(am13e_telem_frame_push(&s,0x5aU,out,4U,&n));
    assert(n==1U&&out[0]==0x5aU);
    am13e_telem_frame_init(&s,6U);
    assert(!am13e_telem_frame_push(&s,0U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x80U,out,4U,&n));
    assert(am13e_telem_frame_push(&s,0x8cU,out,4U,&n));
    assert(n==2U&&out[0]==0x80U&&out[1]==0x8cU);
    assert(!am13e_telem_frame_push(&s,0x80U,out,4U,&n));
    assert(!am13e_telem_frame_push(&s,0x80U,out,4U,&n));
    assert(s.used==1U);
    assert(am13e_telem_frame_push(&s,0x8cU,out,4U,&n));
    assert(n==2U&&out[0]==0x80U&&out[1]==0x8cU);
    /* Half-open gap boundary, after EOT, and 32-bit wrap */
    am13e_telem_frame_init(&s,3U);
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    am13e_telem_frame_gap(&s,7U,8U);
    assert(s.used==1U&&s.aborts==0U);
    am13e_telem_frame_gap(&s,8U,8U);
    assert(s.used==0U&&s.aborts==1U);
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    assert(am13e_telem_frame_push(&s,0x10U,out,4U,&n));
    am13e_telem_frame_gap(&s,20U,8U);
    assert(s.used==0U&&s.aborts==1U);
    assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
    const uint32_t previous=UINT32_MAX-2U;
    const uint32_t now=4U; /* six 16kHz ticks across wrap */
    am13e_telem_frame_gap(&s,(uint32_t)(now-previous),8U);
    assert(s.used==1U);
    am13e_telem_frame_gap(&s,8U,8U);
    assert(s.used==0U&&s.aborts==2U);
    am13e_telem_frame_gap(NULL,8U,8U);
    am13e_telem_frame_gap(&s,8U,0U);
    for(unsigned mode=0U;mode<=6U;++mode){
        if(mode==0U||mode==1U||mode==4U){
            am13e_telem_frame_init(&s,mode);
            assert(!am13e_telem_frame_push(&s,0x7eU,out,4U,&n));
            assert(s.used==0U);
        }
    }
    puts("PASS Rel17 telemetry request framing, repeated start recovery, idle/wrap negatives");
    return 0;
}
