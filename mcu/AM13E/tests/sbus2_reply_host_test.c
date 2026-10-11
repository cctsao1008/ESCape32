#include "sbus2_reply.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    uint8_t request[25]={0};
    request[0]=0x0fU;request[24]=0x04U;
    assert(am13e_sbus2_eligible(request,25U,0U,1U));
    assert(!am13e_sbus2_eligible(request,24U,0U,1U));
    assert(!am13e_sbus2_eligible(request,25U,4U,1U));
    assert(!am13e_sbus2_eligible(request,25U,0U,2U));
    request[24]=0x14U;
    assert(am13e_sbus2_eligible(request,25U,6U,2U));
    const AM13E_Sbus2ReplyValues v={
       .temp1=25,.temp2=-10,.curr=0x3456,.volt=0x5678,
       .csum=0x1234,.erpm=18000,.telem_poles=14,.telphid=2U
    };
    const uint8_t ids[6]={0x53U,0xd3U,0x33U,0xb3U,0x73U,0xf3U};
    AM13E_Sbus2ReplyState s={0};
    uint8_t pkt[3];unsigned delay=0U;
    for(unsigned n=0U;n<6U;++n){
       assert(am13e_sbus2_next_packet(&s,&v,pkt,&delay));
       assert(pkt[0]==ids[n]);
       assert(delay==(n==0U?3200U:660U));
       switch(n){
         case 0U:assert(pkt[1]==125U&&pkt[2]==0x80U);break;
         case 1U:assert(pkt[1]==90U&&pkt[2]==0x80U);break;
         case 2U:assert(pkt[1]==(uint8_t)(0x34U|0x40U)&&pkt[2]==0x56U);break;
         case 3U:assert(pkt[1]==0x56U&&pkt[2]==0x78U);break;
         case 4U:assert(pkt[1]==0x12U&&pkt[2]==0x34U);break;
         case 5U:assert(pkt[1]==(uint8_t)(18000U/42U)&&pkt[2]==(uint8_t)((18000U/42U)>>8));break;
       }
    }
    assert(s.slot_index==6U);
    assert(!am13e_sbus2_next_packet(&s,&v,pkt,&delay));
    AM13E_Sbus2ReplyState bad={0};
    AM13E_Sbus2ReplyValues zero=v;
    zero.telem_poles=0;
    assert(!am13e_sbus2_next_packet(&bad,&zero,pkt,&delay));
    puts("PASS original Rel17 SBUS2 24 slot IDs, 6 data types and 3200/660us schedule");
    return 0;
}
