/* Original EXBUS CRC16 (Rel17 util.c crc16ccitt) and 16-bit
 * little-endian >>3 channel mapping, CRC-valid telemetry recognition.
 */
#include "exbus_receiver.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint16_t ref_crc(const char *buf,int len)
{
    uint16_t crc=0U;
    while(len--){
        uint8_t v=(uint8_t)(crc^(uint8_t)*buf++);
        v^=(uint8_t)(v<<4);
        crc=(uint16_t)((crc>>8)^((uint16_t)v<<8)^
              ((uint16_t)v<<3)^(v>>4));
    }
    return crc;
}
static void finalize(uint8_t *p,unsigned len){
    /* Appending the low/high remainder in Rel17 EXBUS order makes
     * crc16ccitt(complete_frame) == 0 for a correct frame.
     */
    uint16_t c=ref_crc((const char *)p,(int)(len-2U));
    p[len-2U]=(uint8_t)c;
    p[len-1U]=(uint8_t)(c>>8U);
    assert(ref_crc((const char *)p,(int)len)==0U);
}
static void channel(uint8_t *p,unsigned idx,unsigned raw){
    p[6U+idx*2U]=(uint8_t)raw;
    p[7U+idx*2U]=(uint8_t)(raw>>8U);
}
int main(void){
    uint8_t p[64]={0};
    int a=0,b=0;
    bool channels=false,telem=false;
    p[0]=0x3eU;p[1]=0x01U;p[2]=12U;p[3]=0x22U;
    p[4]=0x31U;p[5]=4U;
    channel(p,0U,8000U);channel(p,1U,12000U);
    finalize(p,12U);
    assert(am13e_exbus_decode_frame(p,12U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    assert(channels&&!telem&&a==1000&&b==1500);
    assert(am13e_exbus_decode_frame(p,12U,0U,3U,ref_crc,
             &a,&b,&channels,&telem));
    assert(a==-1&&b==-1);
    assert(!am13e_exbus_decode_frame(p,11U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    p[6]^=1U;
    assert(!am13e_exbus_decode_frame(p,12U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    p[6]^=1U;
    p[2]=8U;p[4]=0x3aU;p[5]=0U;finalize(p,8U);
    assert(am13e_exbus_decode_frame(p,8U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    assert(!channels&&telem&&a==-1&&b==-1);
    p[5]=1U;finalize(p,8U);
    assert(!am13e_exbus_decode_frame(p,8U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    p[5]=0U;p[4]=0x00U;finalize(p,8U);
    assert(am13e_exbus_decode_frame(p,8U,1U,2U,ref_crc,
             &a,&b,&channels,&telem));
    assert(!channels&&!telem);
    /* Exhaustively match source-defined 16-bit channel >>3 values. */
    p[2]=12U;p[4]=0x31U;p[5]=4U;
    for(unsigned raw=0U;raw<=65535U;++raw){
        channel(p,0U,raw);channel(p,1U,65535U-raw);
        finalize(p,12U);
        assert(am13e_exbus_decode_frame(p,12U,1U,2U,ref_crc,
                 &a,&b,&channels,&telem));
        assert(a==(int)(raw>>3U));
        assert(b==(int)((65535U-raw)>>3U));
    }
    puts("PASS Rel17 EXBUS CRC16, 65536 channel codes, channel/telemetry");
    return 0;
}
