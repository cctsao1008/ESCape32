#include "telem_mode_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    const uint32_t original_baud[7] = {115200,115200,115200,57600,416666,38400,19200};
    const int callback_rx[7] = {0,0,1,1,0,1,1};
    const int inverted[7] = {0,0,0,1,0,0,0};
    const int turns[7] = {0,0,0,1,0,0,1};
    for (int mode=0; mode < 7; ++mode) {
        AM13E_TelemModePlan p = {0};
        assert(am13e_telem_mode_plan(mode,&p));
        assert(p.baud_rate == original_baud[mode]);
        assert(p.rx_protocol == callback_rx[mode]);
        assert(p.inverted_line == inverted[mode]);
        assert(p.requires_turnaround == turns[mode]);
        assert(p.rx_timeout_bits == (mode==3?26:10));
    }
    AM13E_TelemModePlan p = {0};
    assert(!am13e_telem_mode_plan(-1,&p));
    assert(!am13e_telem_mode_plan(7,&p));
    assert(!am13e_telem_mode_plan(0,NULL));
    puts("AM13E Rel17 telemetry UART mode plan (7 modes) PASS");
    return 0;
}
