#include "motor_phase_plan.h"
#include "motor_aq_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static const int positive[6]={36,25,33,2,26,4};
static const int negative[6]={10,2,20,12,33,17};
static const int comparator[6]={5,3,6,1,7,2};
int main(void)
{
    unsigned checks=0;
    for(int reverse=0;reverse<2;++reverse)
    for(int step=0;step<6;++step){
        AM13E_SixstepPlan p;
        assert(am13e_motor_plan_sixstep(positive[step],negative[step],
              comparator[step]^(reverse?4:0),0,reverse,&p));
        AM13E_MotorAQShadowPlan aq;
        assert(am13e_motor_aq_plan_sixstep(&p,&aq));
        assert(am13e_motor_aq_plan_validate(&aq));
        int cntPositive=0,cntSink=0,cntFloat=0;
        for(unsigned i=0;i<3U;++i){
            const uint16_t aa=aq.action[i*2],bb=aq.action[i*2+1];
            switch(p.phase[i]){
                case AM13E_PHASE_POSITIVE_PWM:
                    assert(aa==0x12U && bb==0U);++cntPositive;break;
                case AM13E_PHASE_NEGATIVE_SINK:
                    assert(aa==0U && bb==0x2U);++cntSink;break;
                case AM13E_PHASE_FLOATING:
                    assert(aa==0U && bb==0U);++cntFloat;break;
                default: assert(0);
            }
        }
        assert(cntPositive==1&&cntSink==1&&cntFloat==1);
        p.damp=1U;
        assert(am13e_motor_aq_plan_sixstep(&p,&aq));
        assert(am13e_motor_aq_plan_validate(&aq));
        for(unsigned i=0;i<3U;++i){
            const uint16_t aa=aq.action[i*2],bb=aq.action[i*2+1U];
            if(p.phase[i]==AM13E_PHASE_POSITIVE_PWM)
                assert(aa==0x12U && bb==0x201U);
            else if(p.phase[i]==AM13E_PHASE_NEGATIVE_SINK)
                assert(aa==0U && bb==0x2U);
            else
                assert(aa==0U && bb==0U);
        }
        /* A high-side PWM plus constantly asserted low-side sink
         * violates mutually exclusive phase roles.
         */
        for(unsigned i=0;i<3U;++i)
            if(p.phase[i]==AM13E_PHASE_POSITIVE_PWM) {
                aq.action[i*2+1U]=0x2U;
                assert(!am13e_motor_aq_plan_validate(&aq));
                break;
            }
        ++checks;
    }
    for(int c=0;c<8;++c){
        AM13E_SixstepPlan coast;
        AM13E_MotorAQShadowPlan aq;
        assert(am13e_motor_plan_sixstep(0,0,c,0,0,&coast));
        assert(am13e_motor_aq_plan_sixstep(&coast,&aq));
        for(unsigned i=0;i<6U;++i)assert(aq.action[i]==0U);
        ++checks;
    }
    AM13E_MotorAQShadowPlan aq={{0U}};
    assert(!am13e_motor_aq_plan_sixstep(NULL,&aq));
    AM13E_SixstepPlan p={{AM13E_PHASE_FLOATING,AM13E_PHASE_FLOATING,AM13E_PHASE_FLOATING},0,0,0};
    assert(!am13e_motor_aq_plan_sixstep(&p,NULL));
    p.phase[1]=(AM13E_PhaseRole)99;
    assert(!am13e_motor_aq_plan_sixstep(&p,&aq));
    aq.action[0]=UINT16_C(0x1000);
    assert(!am13e_motor_aq_plan_validate(&aq));
    assert(!am13e_motor_aq_plan_validate(NULL));
    printf("E62 Rel17 six-step AQ: %u sequence/coast pairs, complementary image host PASS; physical damp requires verified board DB\n",checks);
    return 0;
}
