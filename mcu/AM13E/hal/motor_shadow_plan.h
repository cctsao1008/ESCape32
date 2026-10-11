/* E1-AR - inactive six-output MCPWM0 shadow preflight ONLY.
 * The plan is register-level data, NOT an authorized bridge waveform.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint16_t period;
    uint16_t compare[6]; /* MCPWM0 1A, 1B, 2A, 2B, 3A, 3B */
} AM13E_MotorShadowPlan;
int am13e_motor_shadow_validate(const AM13E_MotorShadowPlan *plan);
