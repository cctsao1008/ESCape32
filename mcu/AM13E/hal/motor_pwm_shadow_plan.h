/* E1-AS: compose Rel17 frequency/duty policies into an inactive MCPWM0
 * shadow-register image. NO output/AQ/dead-band/Trip Zone activation.
 * The board must independently qualify dead-time ticks and polarities.
 */
#pragma once
#include "motor_shadow_plan.h"
#include <stdint.h>
typedef struct {
    uint32_t clock_hz;
    int freq_min_khz;
    int freq_max_khz;
    int ertm_us;
    int logical_duty;
    int board_dead_ticks;
    int lock;
    int running;
    int damp;
    int brushed;
    int full_duty;
} AM13E_MotorPwmShadowInputs;
/* Returns zero if any compare is unrepresentable with the selected MCPWM
 * period (notably FULL_DUTY 100%). That limitation must be resolved in the
 * real output driver rather than silently clipping a valid Rel17 command.
 */
int am13e_motor_pwm_shadow_plan(const AM13E_MotorPwmShadowInputs *input,
                                AM13E_MotorShadowPlan *out);
