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
    /* ECAP final RX edge, not ISR-entry time, owns 30us deadline.
     * At 200MHz capture / 100MHz TX clock: 30us=6000 ECAP ticks;
     * ISR at +5us leaves exactly 2500 TX timer ticks.
     */
    const uint32_t cap=200000000U,tx=100000000U;
    assert(am13e_bidir_turnaround_ticks(100000U,101000U,cap,tx)==2500U);
    assert(am13e_bidir_turnaround_ticks(100000U,105800U,cap,tx)==100U);
    assert(am13e_bidir_turnaround_ticks(100000U,105968U,cap,tx)==16U);
    assert(am13e_bidir_turnaround_ticks(100000U,105970U,cap,tx)==0U);
    assert(am13e_bidir_turnaround_ticks(100000U,106000U,cap,tx)==0U);
    assert(am13e_bidir_turnaround_ticks(100000U,106001U,cap,tx)==0U);
    assert(am13e_bidir_turnaround_ticks(0xfffffff0U,0x3d8U,cap,tx)==2500U);
    assert(am13e_bidir_turnaround_ticks(100000U,101000U,0U,tx)==0U);
    assert(am13e_bidir_turnaround_ticks(100000U,101000U,cap,0U)==0U);
    /* Unknown/implausibly huge 30us period must not overflow TIMG4. */
    assert(am13e_bidir_turnaround_ticks(0U,0U,cap,UINT32_MAX)==0U);
    puts("AM13E BiDShot TIMG4/DMA toggle plan 4096 payloads PASS");
    return 0;
}
