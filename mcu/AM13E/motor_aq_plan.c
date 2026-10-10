#include "motor_aq_plan.h"
#include <stddef.h>
/* MCPWM AQ events, per TI hw_mcpwm.h: ZERO bits 1:0, CMPA up bits
 * 5:4, CMPB up bits 9:8. Complementary AQ alone is NEVER a qualified
 * safe gate waveform: runtime must separately require the actual
 * board-configured RED/FED deadband and power-stage Trip before enable.
 */
#define AQ_HIGH_ZERO      UINT16_C(0x0002)
#define AQ_LOW_ZERO       UINT16_C(0x0001)
#define AQ_LOW_UP_CMPA    UINT16_C(0x0010)
#define AQ_HIGH_UP_CMPB   UINT16_C(0x0200)
#define AQ_PWM_A          (AQ_HIGH_ZERO|AQ_LOW_UP_CMPA)
#define AQ_SINK_B         AQ_HIGH_ZERO
#define AQ_FREEWHEEL_B    (AQ_LOW_ZERO|AQ_HIGH_UP_CMPB)
int am13e_motor_aq_plan_validate(const AM13E_MotorAQShadowPlan *plan)
{
    if (plan == NULL) return 0;
    unsigned pwm = 0U, sink = 0U, freewheel = 0U;
    for (unsigned i = 0U; i < 3U; ++i) {
        const uint16_t a = plan->action[2U*i];
        const uint16_t b = plan->action[2U*i+1U];
        if (a != 0U && a != AQ_PWM_A) return 0;
        if (b != 0U && b != AQ_SINK_B && b != AQ_FREEWHEEL_B)
            return 0;
        /* Only the verified complementary A-PWM/B-inverse pair may
         * use both outputs of one phase. Floating/sink leg never PWM.
         */
        if (a != 0U && b != 0U && b != AQ_FREEWHEEL_B)
            return 0;
        if (a == 0U && b == AQ_FREEWHEEL_B) return 0;
        pwm += a != 0U;
        sink += b == AQ_SINK_B;
        freewheel += b == AQ_FREEWHEEL_B;
    }
    return (pwm==0U && sink==0U && freewheel==0U) ||
           (pwm==1U && sink==1U && freewheel<=1U);
}
int am13e_motor_aq_plan_sixstep(const AM13E_SixstepPlan *phase,
                               AM13E_MotorAQShadowPlan *out)
{
    if (phase == NULL || out == NULL || phase->damp>1U) return 0;
    AM13E_MotorAQShadowPlan p={{0U,0U,0U,0U,0U,0U}};
    for (unsigned i=0U;i<3U;++i) {
        switch (phase->phase[i]) {
            case AM13E_PHASE_FLOATING: break;
            case AM13E_PHASE_POSITIVE_PWM:
                p.action[2U*i]=AQ_PWM_A;
                if (phase->damp) p.action[2U*i+1U]=AQ_FREEWHEEL_B;
                break;
            case AM13E_PHASE_NEGATIVE_SINK:
                p.action[2U*i+1U]=AQ_SINK_B;
                break;
            default:return 0;
        }
    }
    if (!am13e_motor_aq_plan_validate(&p)) return 0;
    *out=p;
    return 1;
}
