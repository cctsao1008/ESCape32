/* Host fixtures: original 32-byte iBUS format and channel selection. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "ibus_receiver.h"
static void checksum(uint8_t frame[32]){
    unsigned u=0xff9fU;
    for(unsigned i=2U;i<30U;++i)u-=frame[i];
    frame[30]=(uint8_t)u;frame[31]=(uint8_t)(u>>8U);
}
int main(void){
    uint8_t f[32]={0x20U,0x40U};int throttle=0,brake=0;
    for(unsigned ch=1U;ch<=14U;++ch){
        unsigned v=1000U+ch*50U;
        f[ch*2U]=(uint8_t)v;f[ch*2U+1U]=(uint8_t)(v>>8U);
    }
    checksum(f);
    assert(am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,&brake));
    assert(throttle==1050&&brake==1100);
    assert(am13e_ibus_decode_channels(f,32U,14U,0U,&throttle,&brake));
    assert(throttle==1700&&brake==-1);
    assert(am13e_ibus_decode_channels(f,32U,0U,15U,&throttle,&brake));
    assert(throttle==-1&&brake==-1);
    assert(am13e_ibus_decode_channels(f,32U,1U,1U,&throttle,&brake));
    assert(throttle==1050&&brake==-1);
    assert(!am13e_ibus_decode_channels(NULL,32U,1U,2U,&throttle,&brake));
    assert(!am13e_ibus_decode_channels(f,31U,1U,2U,&throttle,&brake));
    assert(!am13e_ibus_decode_channels(f,33U,1U,2U,&throttle,&brake));
    assert(!am13e_ibus_decode_channels(f,32U,1U,2U,NULL,&brake));
    assert(!am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,NULL));
    f[0]=0xffU;assert(!am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,&brake));
    f[0]=0x20U;f[1]=0xffU;
    assert(!am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,&brake));
    f[1]=0x40U;checksum(f);
    f[13]^=1U;assert(!am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,&brake));
    f[13]^=1U;
    for(unsigned v=0U;v<=4095U;++v){
        f[2]=(uint8_t)v;f[3]=(uint8_t)(v>>8U);checksum(f);
        assert(am13e_ibus_decode_channels(f,32U,1U,2U,&throttle,&brake));
        assert(throttle==(int)v&&brake==1100);
    }
    puts("PASS original Rel17 iBUS 32-byte CRC/channels, 4096 channel values");
    return 0;
}
