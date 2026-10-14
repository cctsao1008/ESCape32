#include "motor_frequency_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    AM13E_MotorFrequencyPlan plan={0};
    assert(am13e_motor_frequency_plan(100000000U,16,24,0,&plan));
    assert(plan.period_ticks==6250U && plan.freq_min_period_ticks==6250U);
    assert(plan.freq_max_period_ticks==4166U);
    assert(am13e_motor_frequency_plan(100000000U,16,24,999,&plan));
    assert(plan.period_ticks==4166U);
    assert(am13e_motor_frequency_plan(100000000U,16,24,1000,&plan));
    assert(plan.period_ticks==4166U);
    assert(am13e_motor_frequency_plan(100000000U,16,24,1500,&plan));
    assert(plan.period_ticks==5208U);
    assert(am13e_motor_frequency_plan(100000000U,16,24,2000,&plan));
    assert(plan.period_ticks==6250U);
    assert(am13e_motor_frequency_plan(100000000U,16,24,65535,&plan));
    assert(plan.period_ticks==6250U);
    assert(am13e_motor_frequency_plan(100000000U,24,24,1500,&plan));
    assert(plan.period_ticks==4166U);
    assert(!am13e_motor_frequency_plan(100000000U,16,24,-1,&plan));
    assert(!am13e_motor_frequency_plan(100000000U,15,24,1500,&plan));
    assert(!am13e_motor_frequency_plan(100000000U,16,97,1500,&plan));
    assert(!am13e_motor_frequency_plan(100000000U,48,16,1500,&plan));
    assert(!am13e_motor_frequency_plan(100000000U,16,24,1500,0));
    assert(!am13e_motor_frequency_plan(0,16,24,1500,&plan));
    /* 1GHz/16kHz is 62500 ticks: valid on a 16-bit counter.
     * 2GHz/16kHz is 125000 ticks: overflow must be rejected.
     */
    assert(am13e_motor_frequency_plan(1000000000U,16,24,1500,&plan));
    assert(plan.freq_min_period_ticks == 62500U);
    assert(!am13e_motor_frequency_plan(2000000000U,16,24,1500,&plan));
    const uint32_t clocks[]={1,999,100000000U,1000000000U,3145727999U,UINT32_MAX};
    const int times[]={0,1,1000,1001,1500,1999,2000,2147483647};
    for(unsigned c=0;c<sizeof clocks/sizeof clocks[0];++c)
    for(int min=16;min<=48;++min)for(int max=min;max<=96;++max)
    for(unsigned t=0;t<sizeof times/sizeof times[0];++t){
        uint64_t lo=(uint64_t)clocks[c]/1000U/(unsigned)min;
        uint64_t hi=(uint64_t)clocks[c]/1000U/(unsigned)max;
        int valid=lo>=2 && lo<=65535 && hi>=2 && hi<=65535;
        assert(am13e_motor_frequency_plan(clocks[c],min,max,times[t],&plan)==valid);
        if(!valid)continue;
        uint64_t selected=times[t]==0 || times[t]>=2000?lo:times[t]<=1000?hi:
            hi+(uint64_t)(times[t]-1000)*(lo-hi)/1000U;
        assert(plan.period_ticks==selected && plan.nominal_frequency_hz==
            ((uint64_t)clocks[c]+selected/2U)/selected);
    }
    puts("AM13E Rel17 variable PWM period frequency plan PASS");
    return 0;
}
