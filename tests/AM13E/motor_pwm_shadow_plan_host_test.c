#include "motor_pwm_shadow_plan.h"
#include "motor_frequency_plan.h"
#include "motor_duty_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    unsigned cases=0U;
    AM13E_MotorPwmShadowInputs input={
        .clock_hz=100000000U,.freq_min_khz=16,.freq_max_khz=24,
        .ertm_us=1500,.logical_duty=1000,.board_dead_ticks=28,
        .lock=0,.running=1,.damp=1,.brushed=0,.full_duty=0
    };
    AM13E_MotorShadowPlan result={0};
    assert(am13e_motor_pwm_shadow_plan(&input,&result));
    assert(result.period==5208U);
    assert(result.compare[0]==2618U); /* 28+(1000*(5208-28)/2000) */
    for (int fd=0;fd<2;++fd)
    for (int damp=0;damp<2;++damp)
    for (int running=0;running<2;++running)
    for (int lock=0;lock<2;++lock)
    for (int brushed=0;brushed<2;++brushed)
    for (int ertm=0;ertm<=3000;ertm+=125)
    for (int duty=0;duty<=2000;duty+=125) {
        input.full_duty=fd;input.damp=damp;input.running=running;
        input.lock=lock;input.brushed=brushed;
        input.ertm_us=ertm;input.logical_duty=duty;
        AM13E_MotorFrequencyPlan f;
        AM13E_MotorDutyPlan d;
        assert(am13e_motor_frequency_plan(input.clock_hz,
            input.freq_min_khz,input.freq_max_khz,ertm,&f));
        assert(am13e_motor_duty_plan(f.period_ticks,input.clock_hz,
            duty,input.board_dead_ticks,lock,running,damp,brushed,fd,&d));
        const int representable=d.compare_ticks<=d.period_register_ticks;
        assert(am13e_motor_pwm_shadow_plan(&input,&result)==representable);
        if(representable){
            assert(result.period==d.period_register_ticks);
            for(unsigned i=0;i<6U;++i)
                assert(result.compare[i]==d.compare_ticks);
        }
        ++cases;
    }
    input.full_duty=0;input.brushed=0;input.lock=0;
    input.damp=0;input.running=0;input.ertm_us=0;
    input.logical_duty=0;
    input.freq_max_khz=12;assert(!am13e_motor_pwm_shadow_plan(&input,&result));
    input.freq_max_khz=24;
    input.board_dead_ticks=-1;assert(!am13e_motor_pwm_shadow_plan(&input,&result));
    input.board_dead_ticks=28;
    input.full_duty=2;assert(!am13e_motor_pwm_shadow_plan(&input,&result));
    input.full_duty=0;
    assert(!am13e_motor_pwm_shadow_plan(NULL,&result));
    assert(!am13e_motor_pwm_shadow_plan(&input,NULL));
    printf("E1-AS Rel17 PWM->six shadow registers %u scenarios PASS\n",cases);
    return 0;
}
