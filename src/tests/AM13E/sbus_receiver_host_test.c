/* Deterministic original Rel17 25B SBUS packed 11-bit channels. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sbus_receiver.h"
static void pack(uint8_t frame[25],unsigned ch,unsigned value){
    const unsigned pos=ch*11U;
    for(unsigned b=0U;b<11U;++b){
        unsigned bit=pos+b;
        uint8_t mask=(uint8_t)(1U<<(bit&7U));
        if(value&(1U<<b))frame[1U+(bit>>3U)]|=mask;
        else frame[1U+(bit>>3U)]&=(uint8_t)~mask;
    }
}
static int value_us(unsigned code){
    return (int)((code*5U>>3U)+880U);
}
int main(void){
    uint8_t frame[25]={0};
    frame[0]=0x0fU;
    frame[24]=0x04U;
    int throttle=0,brake=0;
    for(unsigned ch=0U;ch<16U;++ch)pack(frame,ch,ch*123U);
    assert(am13e_sbus_decode_channels(frame,25U,1U,2U,&throttle,&brake));
    assert(throttle==value_us(0U)&&brake==value_us(123U));
    assert(am13e_sbus_decode_channels(frame,25U,16U,0U,&throttle,&brake));
    assert(throttle==value_us(15U*123U)&&brake==-1);
    assert(am13e_sbus_decode_channels(frame,25U,0U,20U,&throttle,&brake));
    assert(throttle==-1&&brake==-1);
    assert(!am13e_sbus_decode_channels(NULL,25U,1U,2U,&throttle,&brake));
    assert(!am13e_sbus_decode_channels(frame,24U,1U,2U,&throttle,&brake));
    assert(!am13e_sbus_decode_channels(frame,26U,1U,2U,&throttle,&brake));
    assert(!am13e_sbus_decode_channels(frame,25U,1U,2U,NULL,&brake));
    assert(!am13e_sbus_decode_channels(frame,25U,1U,2U,&throttle,NULL));
    frame[0]=0;
    assert(!am13e_sbus_decode_channels(frame,25U,1U,2U,&throttle,&brake));
    frame[0]=0x0fU;
    /* Original sbusfunc() accepts any frame[24]; do NOT invent
     * a new payload checksum or stricter end-byte requirement.
     */
    frame[24]=0xffU;
    assert(am13e_sbus_decode_channels(frame,25U,1U,2U,&throttle,&brake));
    for(unsigned v=0U;v<=2047U;++v){
        for(unsigned ch=0U;ch<16U;++ch)pack(frame,ch,v);
        assert(am13e_sbus_decode_channels(frame,25U,1U,16U,&throttle,&brake));
        assert(throttle==value_us(v)&&brake==value_us(v));
    }
    puts("PASS original Rel17 SBUS 16x11-bit channels, 2048 values and framing");
    return 0;
}
