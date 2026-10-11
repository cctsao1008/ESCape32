/* Rel17 Six-step -> MCPWM0 AQ Shadow: coast, sink, PWM and damp
 * complementary images. Damp is permitted by the physical Runtime only
 * under explicit board dead-band configuration and forced pad isolation.
 * These logical event words do not authorize physical gate output.
 */
#pragma once
#include "motor_phase_plan.h"
#include <stdint.h>
typedef struct {
    uint16_t action[6]; /* U-A/U-B, V-A/V-B, W-A/W-B */
} AM13E_MotorAQShadowPlan;
int am13e_motor_aq_plan_sixstep(const AM13E_SixstepPlan *phase,
                               AM13E_MotorAQShadowPlan *out);
int am13e_motor_aq_plan_validate(const AM13E_MotorAQShadowPlan *plan);

/* Original Rel17 lock=0 idle: three complementary low-side PWM images.
 * This is a register-level stage; pads remain Hi-Z until HW qualification.
 */
int am13e_motor_aq_plan_drag_brake(AM13E_MotorAQShadowPlan *out);
