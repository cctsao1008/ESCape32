/* E1-AT: candidate Rel17 six-step -> MCPWM0 AQ SHADOW event words.
 * These are internal register images, NOT board-qualified gate drive.
 * For physical safety this stage supports only unpowered/coast PWM roles.
 * Damp/complementary is rejected until dead-band/Trip Zone are qualified.
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
