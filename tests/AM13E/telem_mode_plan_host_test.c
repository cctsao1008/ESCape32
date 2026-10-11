#include "telem_mode_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    const uint32_t original_baud[7] = {115200,115200,115200,57600,416666,38400,19200};
    const int callback_rx[7] = {0,0,1,1,0,1,1};
    const int inverted[7] = {0,0,0,1,0,0,0};
    const int turns[7] = {0,0,1,1,0,1,1};
    for (int mode=0; mode < 7; ++mode) {
        AM13E_TelemModePlan p = {0};
        assert(am13e_telem_mode_plan(mode,&p));
        assert(p.baud_rate == original_baud[mode]);
        assert(p.rx_protocol == callback_rx[mode]);
        assert(p.inverted_line == inverted[mode]);
        assert(p.requires_turnaround == turns[mode]);
        assert(p.legacy_single_wire == 1U);
        assert(p.rx_timeout_bits == (mode==3?26:10));
    }
    AM13E_TelemModePlan p = {0};
    /* SDK RXTOSEL 4-bit limitation: S.Port RTOR=26 must not silently
     * truncate. Other RX modes must fit, without claiming identical units.
     */
    uint8_t reg = 0U;
    assert(am13e_telem_mode_plan(3,&p));
    assert(!am13e_telem_rx_timeout_register(&p,&reg));
    for(int mode=2;mode<=6;++mode){
        if(mode==3||mode==4) continue;
        assert(am13e_telem_mode_plan(mode,&p));
        assert(am13e_telem_rx_timeout_register(&p,&reg));
        assert(reg==10U);
    }
    assert(am13e_telem_mode_plan(0,&p));
    assert(!am13e_telem_rx_timeout_register(&p,&reg));
    assert(!am13e_telem_rx_timeout_register(NULL,&reg));
    assert(!am13e_telem_rx_timeout_register(&p,NULL));
    assert(!am13e_telem_mode_plan(-1,&p));
    assert(!am13e_telem_mode_plan(7,&p));
    assert(!am13e_telem_mode_plan(0,NULL));
    puts("AM13E Rel17 telemetry UART/HDSEL/timeout parity (7 modes) PASS");
    return 0;
}
