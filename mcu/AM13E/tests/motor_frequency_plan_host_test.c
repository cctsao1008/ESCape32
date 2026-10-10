#include "motor_frequency_plan.h"
#include <assert.h>
#include <stdio.h>
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
    puts("AM13E Rel17 variable PWM period frequency plan PASS");
    return 0;
}
