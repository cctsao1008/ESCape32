/* Test original CRSF 416666 baud receive-frame semantics: source
 * crc8dvbs2 residue and getchan1 16x11-bit -> throttle microseconds.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "crsf_receiver.h"
static uint8_t crc8_reference(const uint8_t *data,unsigned n)
{
    uint8_t crc=0U;
    for(unsigned i=0U;i<n;++i){
        crc^=data[i];
        for(unsigned j=0U;j<8U;++j)
            crc=(uint8_t)((crc<<1U)^((crc&0x80U)?0xd5U:0U));
    }
    return crc;
}
static void pack_channel(uint8_t *data,unsigned ch,unsigned v)
{
    for(unsigned b=0;b<11U;++b){
        const unsigned bit=ch*11U+b;
        const uint8_t mask=(uint8_t)(1U<<(bit&7U));
        if(v&(1U<<b)) data[bit>>3U]|=mask;
        else data[bit>>3U]&=(uint8_t)~mask;
    }
}
static void finalize(uint8_t *p,unsigned n)
{
    p[n-1U]=crc8_reference(p+2U,n-3U);
}
static void run(void)
{
    uint8_t p[64]={0};int throttle=0,brake=0;bool channels=false;
    p[0]=0xc8U;p[1]=24U;p[2]=0x16U;
    for(unsigned ch=0;ch<16U;++ch)pack_channel(p+3U,ch,100U+ch*110U);
    finalize(p,26U);
    assert(am13e_crsf_decode_frame(p,26U,1U,16U,&throttle,&brake,&channels));
    assert(channels&&throttle==((100*5>>3)+880) &&
           brake==(((100+15*110)*5>>3)+880));
    assert(am13e_crsf_decode_frame(p,26U,0U,17U,&throttle,&brake,&channels));
    assert(channels&&throttle==-1&&brake==-1);
    assert(!am13e_crsf_decode_frame(p,25U,1U,2U,&throttle,&brake,&channels));
    assert(!am13e_crsf_decode_frame(p,65U,1U,2U,&throttle,&brake,&channels));
    assert(!am13e_crsf_decode_frame(NULL,26U,1U,2U,&throttle,&brake,&channels));
    assert(!am13e_crsf_decode_frame(p,26U,1U,2U,NULL,&brake,&channels));
    assert(!am13e_crsf_decode_frame(p,26U,1U,2U,&throttle,NULL,&channels));
    assert(!am13e_crsf_decode_frame(p,26U,1U,2U,&throttle,&brake,NULL));
    p[8]^=1U;
    assert(!am13e_crsf_decode_frame(p,26U,1U,2U,&throttle,&brake,&channels));
    p[8]^=1U;
    for(unsigned raw=0;raw<=2047U;++raw){
        for(unsigned ch=0;ch<16U;++ch)pack_channel(p+3U,ch,raw);
        finalize(p,26U);
        assert(am13e_crsf_decode_frame(p,26U,1U,16U,&throttle,&brake,&channels));
        assert(channels&&throttle==(int)((raw*5U>>3U)+880U) &&
               brake==(int)((raw*5U>>3U)+880U));
    }
    /* Rel17 accepts CRC-valid unknown/command types, without
     * applying channel policy; TX command replies remain pending.
     */
    p[2]=0x28U;finalize(p,26U);
    assert(am13e_crsf_decode_frame(p,26U,1U,16U,&throttle,&brake,&channels));
    assert(!channels&&throttle==-1&&brake==-1);
    p[1]=2U;p[2]=0x29U;finalize(p,4U);
    assert(am13e_crsf_decode_frame(p,4U,1U,2U,&throttle,&brake,&channels));
    assert(!channels);
    p[1]=63U;
    assert(!am13e_crsf_decode_frame(p,4U,1U,2U,&throttle,&brake,&channels));
    puts("PASS CRSF CRC8-DVB-S2, length, unknown types, 16x2048 channel values");
}
int main(void){run();return 0;}
