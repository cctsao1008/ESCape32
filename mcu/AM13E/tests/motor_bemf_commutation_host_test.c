/* Host integration of the REAL ECAP1 sample plan, original sixstep
 * topology, AQ images, zero-cross policy and TIMG12 delay math.
 */
#include "motor_bemf_event_plan.h"
#include "motor_bemf.h"
#include "motor_phase_plan.h"
#include "motor_aq_plan.h"
#include "motor_timer_math.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
static const int pos[6]={36,25,33,2,26,4};
static const int neg[6]={10,2,20,12,33,17};
static const int cmp[6]={5,3,6,1,7,2};
int main(void) {
    const uint32_t per_ms=100000U;
    const uint32_t timeout=AM13E_BEMF_REL17_TIMEOUT_US*100U;
    int us=0;
    assert(am13e_bemf_sample_plan(0,per_ms,timeout,&us)==0);
    assert(am13e_bemf_sample_plan(1000,0,timeout,&us)==0);
    assert(am13e_bemf_sample_plan(1000,per_ms,0,&us)==0);
    assert(am13e_bemf_sample_plan(1000,per_ms,timeout,NULL)==0);
    assert(am13e_bemf_sample_plan(timeout-1,per_ms,timeout,&us)==1);
    assert(us==32768);
    assert(am13e_bemf_sample_plan(timeout,per_ms,timeout,&us)==2);
    assert(am13e_bemf_sample_plan(UINT32_MAX,per_ms,timeout,&us)==2);
    unsigned simulated=0;
    for(int reverse=0;reverse<2;++reverse)
    for(int damp=0;damp<2;++damp)
    for(int phase=0;phase<6;++phase) {
        AM13E_SixstepPlan p;
        AM13E_MotorAQShadowPlan aq;
        assert(am13e_motor_plan_sixstep(pos[phase],neg[phase],
             cmp[phase]^(reverse?4:0),damp,reverse,&p));
        assert(p.comparator_code==(unsigned)(cmp[phase]^(reverse?4:0)));
        assert(am13e_motor_aq_plan_sixstep(&p,&aq));
        assert(am13e_motor_aq_plan_validate(&aq));
        unsigned drive=0,sink=0,floating=0;
        for(unsigned i=0;i<3;++i){
            drive+=(p.phase[i]==AM13E_PHASE_POSITIVE_PWM);
            sink+=(p.phase[i]==AM13E_PHASE_NEGATIVE_SINK);
            floating+=(p.phase[i]==AM13E_PHASE_FLOATING);
        }
        assert(drive==1&&sink==1&&floating==1);
        AM13E_BemfPolicyPlan plan={0};
        assert(am13e_bemf_sample_plan(40000,per_ms,timeout,&us)==1);
        assert(us==400);
        assert(am13e_bemf_policy_plan(1000,1200,us,8,&plan)==0);
        assert(am13e_bemf_sample_plan(80000,per_ms,timeout,&us)==1);
        assert(us==800);
        assert(am13e_bemf_policy_plan(1000,1200,us,8,&plan)==1);
        assert(plan.interval_us==950 && plan.delay_us==356 && plan.fast==0);
        assert(am13e_motor_us_to_timer_ticks((uint32_t)plan.delay_us,
               100000000U)==35600U);
        assert(am13e_bemf_sample_plan(timeout,per_ms,timeout,&us)==2);
        ++simulated;
    }
    assert(simulated==24U);
    AM13E_BemfPolicyPlan plan={0};
    assert(am13e_bemf_policy_plan(1000,1000,1600,0,&plan)==1 &&
           plan.fast==1 && plan.interval_us==1150);
    assert(am13e_bemf_policy_plan(1000,2000,1600,32,&plan)==1 &&
           plan.fast==0 && plan.delay_us==1);
    assert(am13e_bemf_policy_plan(0,1000,1000,8,&plan)==-1);
    assert(am13e_bemf_policy_plan(INT_MAX/2,1000,1000,8,&plan)==-1);
    assert(am13e_bemf_policy_plan(1000,1000,1000,33,&plan)==-1);
    assert(am13e_bemf_policy_plan(1000,1000,1000,8,NULL)==-1);
    puts("PASS: ECAP1 edge/early/timeout -> 6-step x CW/CCW x damp -> TIMG12");
    return 0;
}
