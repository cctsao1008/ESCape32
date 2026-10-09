#include "bidir_timing.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint8_t levels[AM13E_BIDIR_DMA_LEVELS]={0};
    uint32_t words[AM13E_BIDIR_DMA_TRANSFERS];
    for(unsigned k=0U;k<4096U;++k){
        unsigned frame=(k&0xfffU)<<4U,crc=0U;
        for(unsigned v=frame;v;v>>=4U)crc^=v;
        frame|=(crc^0xfU)&0xfU;
        am13e_bidir_encode_frame((uint16_t)frame,levels);
        am13e_bidir_toggle_plan(levels,1U<<14U,words);
        unsigned pin=levels[0];
        for(unsigned i=0;i<22U;++i){
            assert(words[i]==0U||words[i]==(1U<<14U));
            pin^=(words[i]!=0U);
            assert(pin==levels[i+1U]);
        }
        assert(words[22]==0U);
    }
    assert(am13e_bidir_tx_period_ticks(667U,100000000U,100000000U)==267U);
    assert(am13e_bidir_tx_period_ticks(333U,100000000U,100000000U)==133U);
    assert(am13e_bidir_tx_period_ticks(167U,100000000U,100000000U)==67U);
    assert(am13e_bidir_tx_period_ticks(1000U,100000000U,100000000U)==0U);
    puts("AM13E BiDShot TIMG4/DMA toggle plan 4096 payloads PASS");
    return 0;
}
