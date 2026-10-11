#include "sbus2_reply.h"
#include <stddef.h>
static const uint8_t slots[24]={
   0x43U,0xc3U,0x23U,0xa3U,0x63U,0xe3U,
   0x53U,0xd3U,0x33U,0xb3U,0x73U,0xf3U,
   0x4bU,0xcbU,0x2bU,0xabU,0x6bU,0xebU,
   0x5bU,0xdbU,0x3bU,0xbbU,0x7bU,0xfbU
};
bool am13e_sbus2_eligible(const uint8_t *p,unsigned len,
                          unsigned telmode,unsigned telphid)
{
    if(!p||len!=25U||p[0]!=0x0fU||telphid<1U||telphid>4U||
       (p[24]&15U)!=4U)return false;
    /* Original SBUS2 schedule is disabled for upstream telmode
     * 2..5 (iBUS/S.Port/CRSF/MSB). Otherwise upper slot nibble must
     * match telphid-1, exactly as sbusfunc().
     */
    return (telmode<2U||telmode>5U)&&
           (unsigned)(p[24]>>4U)==telphid-1U;
}
bool am13e_sbus2_next_packet(AM13E_Sbus2ReplyState *s,
                             const AM13E_Sbus2ReplyValues *v,
                             uint8_t out[3],unsigned *delay)
{
    if(!s||!v||!out||!delay||s->slot_index>=6U||
       v->telphid<1U||v->telphid>4U||v->telem_poles<=0)
        return false;
    const unsigned n=s->slot_index;
    int a=0,b=0;
    switch(n){
      case 0U:
        a=v->temp1+100;b=(a>>8)|0x80;break;
      case 1U:
        a=v->temp2+100;b=(a>>8)|0x80;break;
      case 2U:
        b=v->curr;a=(b>>8)|0x40;break;
      case 3U:
        b=v->volt;a=b>>8;break;
      case 4U:
        b=v->csum;a=b>>8;break;
      case 5U:
        a=v->erpm/(v->telem_poles*3);
        if(a>65535)a=65535;
        b=a>>8;break;
      default:return false;
    }
    out[0]=slots[n+(v->telphid-1U)*6U];
    out[1]=(uint8_t)a;
    out[2]=(uint8_t)b;
    *delay=n==0U?3200U:660U;
    ++s->slot_index;
    return true;
}
