/* ESCape32 Rel17 duty compare policy, peripheral-independent.
 * The caller must supply validated board dead time in PWM timer ticks.
 * This helper NEVER drives a gate or programs MCPWM registers.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint16_t period_register_ticks; /* ARR register after FULL_DUTY arr-- */
    uint16_t compare_ticks; /* Rel17 linear scale(..., min, max) */
    uint16_t lower_compare_ticks;
    uint16_t upper_compare_ticks;
} AM13E_MotorDutyPlan;
/* Preserve original Rel17:
 * lower = (lock || (running && damp)) ? dead_ticks : 0;
 * FULL_DUTY: upper=arr, actual ARR=arr-1;
 * otherwise: upper=(brushed ? arr-1.5us : arr), actual ARR=arr.
 * Invalid constraints fail rather than generate physically illegal PWM.
 */
int am13e_motor_duty_plan(uint32_t period_ticks,uint32_t pwm_clock_hz,
                          int logical_duty,int dead_ticks,
                          int lock,int running,int damp,int brushed,
                          int full_duty,AM13E_MotorDutyPlan *out);
