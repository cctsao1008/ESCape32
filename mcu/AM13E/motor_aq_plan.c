#include "motor_aq_plan.h"
#include <stddef.h>
/* TI MCPWM AQCTLA/B encoding (hw_mcpwm.h): each 2-bit event field.
 * ZERO = bits 1:0. Count-up CMPA = bits 5:4.
 * We intentionally do NOT program an active/freewheeling complement.
 */
#define AQ_HIGH_ZERO UINT16_C(0x0002)
#define AQ_LOW_UP_CMPA UINT16_C(0x0010)
int am13e_motor_aq_plan_validate(const AM13E_MotorAQShadowPlan *plan)
{
    if(plan==NULL)return 0;
    unsigned pwm=0U,sink=0U;
    for(unsigned i=0;i<3U;++i){
        const uint16_t a=plan->action[2U*i];
        const uint16_t b=plan->action[2U*i+1U];
        if(a!=0U && a!=(AQ_HIGH_ZERO|AQ_LOW_UP_CMPA))return 0;
        if(b!=0U && b!=AQ_HIGH_ZERO)return 0;
        if(a!=0U && b!=0U)return 0;
        pwm += a!=0U;
        sink += b!=0U;
    }
    return (pwm==0U && sink==0U) || (pwm==1U && sink==1U);
}
int am13e_motor_aq_plan_sixstep(const AM13E_SixstepPlan *phase,
                               AM13E_MotorAQShadowPlan *out)
{
    if(phase==NULL||out==NULL||phase->damp>0U)return 0;
    AM13E_MotorAQShadowPlan p={{0U,0U,0U,0U,0U,0U}};
    for(unsigned i=0;i<3U;++i){
        switch(phase->phase[i]){
            case AM13E_PHASE_FLOATING:
                /* No action events for either candidate gate. */
                break;
            case AM13E_PHASE_POSITIVE_PWM:
                /* CANDIDATE A: high at ZERO, low at count-up CMPA. */
                p.action[2U*i]=AQ_HIGH_ZERO|AQ_LOW_UP_CMPA;
                break;
            case AM13E_PHASE_NEGATIVE_SINK:
                /* CANDIDATE B: on at ZERO; no complementary drive. */
                p.action[2U*i+1U]=AQ_HIGH_ZERO;
                break;
            default: return 0;
        }
    }
    if(!am13e_motor_aq_plan_validate(&p))return 0;
    *out=p;
    return 1;
}
