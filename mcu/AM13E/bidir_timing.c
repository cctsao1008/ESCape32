#include "bidir_timing.h"
#include <stddef.h>
uint32_t am13e_bidir_tx_period_ticks(uint32_t rx_ticks,uint32_t rx_hz,uint32_t timer_hz)
{
    if (!rx_hz || !timer_hz || !rx_ticks) return 0U;
    const uint64_t rate=((uint64_t)rx_hz+rx_ticks/2U)/rx_ticks;
    const uint32_t nominal[3]={150000U,300000U,600000U};
    int recognized=0;
    for(unsigned i=0U;i<3U;++i)
        if(rate*100U >= (uint64_t)nominal[i]*88U &&
           rate*100U <= (uint64_t)nominal[i]*112U) recognized=1;
    if(!recognized) return 0U;
    const uint64_t numerator=(uint64_t)rx_ticks*timer_hz*2U;
    const uint64_t denominator=(uint64_t)rx_hz*5U;
    if(!denominator) return 0U;
    const uint64_t ticks=(numerator+denominator/2U)/denominator;
    if(ticks<8U || ticks>0xffffU) return 0U;
    return (uint32_t)ticks;
}
void am13e_bidir_toggle_plan(const uint8_t levels[AM13E_BIDIR_DMA_LEVELS],
                            uint32_t pin_mask,uint32_t words[AM13E_BIDIR_DMA_TRANSFERS])
{
    if(!levels || !words) return;
    for(unsigned i=1U;i<AM13E_BIDIR_DMA_LEVELS;++i)
        words[i-1U]=(levels[i]^levels[i-1U])?pin_mask:0U;
    /* Hold the last symbol one full timer period before RX release. */
    words[AM13E_BIDIR_DMA_TRANSFERS-1U]=0U;
}
