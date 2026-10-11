#include "motor_duty_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* Independent reference: Rel17 src/util.c scale() and src/main.c setduty. */
static int rel17_scale(int x,int a1,int a2,int b1,int b2)
{
    if(x<=a1) return b1;
    if(x>=a2) return b2;
    return b1+(x-a1)*(b2-b1)/(a2-a1);
}
int main(void)
{
    AM13E_MotorDutyPlan p = {0};
    unsigned scenarios = 0U;
    for (int full = 0; full < 2; ++full)
    for (int lock = 0; lock < 3; ++lock)
    for (int running = 0; running < 2; ++running)
    for (int damp = 0; damp < 2; ++damp)
    for (int brushed = 0; brushed < 2; ++brushed)
    for (int duty = -100; duty <= 2100; duty += 5) {
        const int arr=6250, dead=28;
        const int lower=(lock || (running && damp)) ? dead : 0;
        const int upper=full ? arr : brushed ? arr-150 : arr;
        const int expected=rel17_scale(duty,0,2000,lower,upper);
        assert(am13e_motor_duty_plan(6250U,100000000U,duty,dead,
                                    lock,running,damp,brushed,full,&p));
        assert(p.lower_compare_ticks == (uint16_t)lower);
        assert(p.upper_compare_ticks == (uint16_t)upper);
        assert(p.compare_ticks == (uint16_t)expected);
        assert(p.period_register_ticks == (uint16_t)(arr-full));
        ++scenarios;
    }
    assert(!am13e_motor_duty_plan(100U,100000000U,500,0,0,1,0,1,0,&p));
    assert(!am13e_motor_duty_plan(300U,100000000U,500,301,1,1,0,0,0,&p));
    assert(!am13e_motor_duty_plan(65536U,100000000U,500,0,0,0,0,0,0,&p));
    assert(!am13e_motor_duty_plan(6250U,0U,500,0,0,0,0,0,0,&p));
    assert(!am13e_motor_duty_plan(6250U,100000000U,500,0,0,0,0,0,0,0));
    assert(!am13e_motor_duty_plan(6250U,100000001U,500,0,0,0,0,0,0,&p));
    printf("AM13E Rel17 duty mapping: %u scenarios PASS\n",scenarios);
    return 0;
}
