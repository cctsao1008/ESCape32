#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "hw_bemf_am13e_policy.h"

int main(void)
{
    static const uint8_t forward[] = {5,3,6,1,7,2};
    for (unsigned i=1;i<=6;++i) {
        uint8_t x = 0;
        assert(am13e_bemf_selector(i,false,&x) && x==forward[i-1]);
        assert(am13e_bemf_selector(i,true,&x) && x==(forward[i-1]^4));
    }
    uint8_t x;
    assert(!am13e_bemf_selector(0,false,&x));
    assert(!am13e_bemf_selector(7,false,&x));
    assert(!am13e_bemf_selector(1,false,0));
    puts("[PASS] exact ESCape32 forward/reverse BEMF selectors");

    am13e_bemf_state_t s = {.interval=1000,.electrical_time=1500,.sync=0,.fast=false};
    uint32_t d=0;
    assert(am13e_bemf_on_capture(&s,499,0,&d)==AM13E_BEMF_IGNORED);
    assert(s.interval==1000 && s.sync==0);
    assert(am13e_bemf_on_capture(&s,1000,0,&d)==AM13E_BEMF_ACCEPTED);
    assert(s.interval==1000 && d==500 && s.sync==1 && !s.fast);
    assert(am13e_bemf_on_capture(&s,1000,8,&d)==AM13E_BEMF_ACCEPTED);
    assert(d==375 && s.sync==2);
    assert(am13e_bemf_on_capture(&s,500,32,&d)==AM13E_BEMF_ACCEPTED);
    assert(d==1);
    assert(am13e_bemf_on_capture(&s,500,33,&d)==AM13E_BEMF_IGNORED);
    puts("[PASS] zero-cross qualification, interval filter and timing adjustment");

    am13e_bemf_on_timeout(&s,0);
    assert(s.sync==0 && !s.fast && s.interval==10000 && s.electrical_time==100000000);
    puts("[PASS] BEMF timeout state reset");
    puts("[PASS] E62 FW1 BEMF timing policy host regression");
    return 0;
}
