/* Original SUMD v1/v3 channel map with source XMODEM CRC. */
#include "hott_receiver.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint16_t ref_crc(const char *data,int len)
{
    uint16_t crc=0U;
    while(len--){
        uint8_t v=(uint8_t)((crc>>8)^(uint8_t)*data++);
        v^=v>>4;
        crc=(uint16_t)((crc<<8)^((uint16_t)v<<12)^
                      ((uint16_t)v<<5)^v);
    }
    return crc;
}
static void make(uint8_t p[64],unsigned channels,unsigned type){
    memset(p,0,64U);p[0]=0xa8U;p[1]=(uint8_t)type;p[2]=(uint8_t)channels;
    for(unsigned i=0U;i<channels;++i){
        const unsigned val=8000U+(i*72U);
        p[3U+i*2U]=(uint8_t)(val>>8U);
        p[4U+i*2U]=(uint8_t)val;
    }
    unsigned n=5U+channels*2U;
    uint16_t crc=ref_crc((const char *)p,(int)(n-2U));
    p[n-2U]=(uint8_t)(crc>>8U);
    p[n-1U]=(uint8_t)crc;
}
int main(void){
    uint8_t p[64]={0};int a=0,b=0;bool channels=false;
    make(p,3U,1U);
    assert(am13e_hott_decode_frame(p,11U,1U,3U,ref_crc,
                      &a,&b,&channels));
    assert(channels&&a==1000&&b==(8000+2*72)/8);
    assert(am13e_hott_decode_frame(p,11U,4U,0U,ref_crc,
                      &a,&b,&channels));
    assert(channels&&a==-1&&b==-1);
    p[6]^=0x10U;
    assert(!am13e_hott_decode_frame(p,11U,1U,3U,ref_crc,
                       &a,&b,&channels));
    p[6]^=0x10U;
    assert(!am13e_hott_decode_frame(p,10U,1U,3U,ref_crc,
                       &a,&b,&channels));
    make(p,12U,3U);
    /* function code at iobuf[count*2-1] = count's last payload byte
     * from original Rel17, not the CRC.
     */
    p[23U]=0U;
    unsigned n=29U;
    uint16_t crc=ref_crc((const char *)p,(int)(n-2U));
    p[n-2U]=(uint8_t)(crc>>8U);p[n-1U]=(uint8_t)crc;
    assert(am13e_hott_decode_frame(p,n,1U,9U,ref_crc,
                       &a,&b,&channels));
    assert(channels&&a==1000&&b==(8000+8*72)/8);
    p[23U]=8U;
    crc=ref_crc((const char *)p,(int)(n-2U));
    p[n-2U]=(uint8_t)(crc>>8U);p[n-1U]=(uint8_t)crc;
    assert(am13e_hott_decode_frame(p,n,1U,9U,ref_crc,
                       &a,&b,&channels));
    assert(!channels);
    /* Exhaustive high bits/endianness on SUMDv1 first channel. */
    make(p,1U,1U);
    for(unsigned raw=0U;raw<=65535U;++raw){
        p[3]=(uint8_t)(raw>>8U);p[4]=(uint8_t)raw;
        crc=ref_crc((const char *)p,5);
        p[5]=(uint8_t)(crc>>8U);p[6]=(uint8_t)crc;
        assert(am13e_hott_decode_frame(p,7U,1U,1U,ref_crc,
                         &a,&b,&channels));
        assert(a==(int)(raw>>3U)&&b==(int)(raw>>3U));
    }
    puts("PASS Rel17 HoTT SUMDv1/v3 source CRC16 and 65536 channel codes");
    return 0;
}
