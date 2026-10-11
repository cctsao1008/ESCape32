#include "motor_phase_plan.h"
#include <assert.h>
#include <stdio.h>
/* Explicit Rel17 hardware roles, not generated from the function itself. */
static const int p[6]={36,25,33,2,26,4};
static const int n[6]={10,2,20,12,33,17};
static const int comp[6]={5,3,6,1,7,2};
int main(void)
{
    AM13E_SixstepPlan o;
    for(int reverse=0;reverse<2;++reverse){
        for(int damp=0;damp<2;++damp){
            for(int step=0;step<6;++step){
                assert(am13e_motor_plan_sixstep(p[step],n[step],
                     comp[step]^(reverse?4:0),damp,reverse,&o));
                int pos=0,neg=0,flt=0;
                for(int i=0;i<3;++i){
                    pos += o.phase[i]==AM13E_PHASE_POSITIVE_PWM;
                    neg += o.phase[i]==AM13E_PHASE_NEGATIVE_SINK;
                    flt += o.phase[i]==AM13E_PHASE_FLOATING;
                }
                assert(pos==1&&neg==1&&flt==1);
                assert(o.damp==(uint8_t)damp);
                assert(o.reverse==(uint8_t)reverse);
            }
        }
    }
    for(int i=0;i<8;++i) {
        assert(am13e_motor_plan_sixstep(0,0,i,0,0,&o));
        for(int bit=0;bit<3;++bit)
            assert(o.phase[bit]==AM13E_PHASE_FLOATING);
    }
    assert(!am13e_motor_plan_sixstep(36,10,3,0,0,&o)); /* wrong code */
    assert(!am13e_motor_plan_sixstep(4,2,5,0,0,&o)); /* truncated masks */
    assert(!am13e_motor_plan_sixstep(1,1,3,0,0,&o));
    assert(!am13e_motor_plan_sixstep(1,2,5,0,0,&o)); /* corrupted */
    assert(!am13e_motor_plan_sixstep(8,1,5,0,0,&o));
    assert(!am13e_motor_plan_sixstep(-1,1,5,0,0,&o));
    assert(!am13e_motor_plan_sixstep(4,2,5,0,0,0));
    puts("Rel17 six-step phase plan: 6 steps x 2 directions x 2 damp, neutral, invalid PASS");
    return 0;
}
