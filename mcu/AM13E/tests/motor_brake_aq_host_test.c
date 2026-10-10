/* Rel17 AM13E lock=0 Drag/Proportional Brake AQ regression.
 * A logical AQ image is not a verified physical power-stage waveform.
 * Host build: cc -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic
 *   -Imcu/AM13E mcu/AM13E/motor_phase_plan.c
 *   mcu/AM13E/motor_aq_plan.c
 *   mcu/AM13E/tests/motor_brake_aq_host_test.c -o /tmp/test_brake
 */
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
    AM13E_MotorAQShadowPlan aq={{0U}};
    assert(!am13e_motor_aq_plan_drag_brake(NULL));
    assert(am13e_motor_aq_plan_drag_brake(&aq));
    assert(am13e_motor_aq_plan_validate(&aq));
    for(unsigned phase=0U;phase<3U;++phase) {
        assert(aq.action[2U*phase]==0U);
        assert(aq.action[2U*phase+1U]==UINT16_C(0x0201));
        /* Never accept an unpaired high-side A with the 3-B brake bank. */
        aq.action[2U*phase]=UINT16_C(0x0012);
        assert(!am13e_motor_aq_plan_validate(&aq));
        aq.action[2U*phase]=0U;
        /* Partial or damaged 3-phase braking is not a legal image. */
        aq.action[2U*phase+1U]=0U;
        assert(!am13e_motor_aq_plan_validate(&aq));
        aq.action[2U*phase+1U]=UINT16_C(0x0201);
    }
    unsigned checks=0U;
    for(unsigned reverse=0U;reverse<2U;++reverse)
    for(unsigned step=0U;step<6U;++step) {
        AM13E_SixstepPlan plan;
        assert(am13e_motor_plan_sixstep(
            positive[step],negative[step],
            comparator[step]^(reverse?4:0),0,(int)reverse,&plan));
        for(unsigned damp=0U;damp<2U;++damp) {
            plan.damp=(uint8_t)damp;
            assert(am13e_motor_aq_plan_sixstep(&plan,&aq));
            assert(am13e_motor_aq_plan_validate(&aq));
            ++checks;
        }
    }
    assert(checks==24U);
    printf("PASS: 3-phase Rel17 brake AQ, isolation conflicts, "
           "%u six-step direction/damp images\n",checks);
    return 0;
}
