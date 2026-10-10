/* ESCape32 Rel17 variable PWM *period* policy; no hardware outputs.
 * Keep the original scale(ertm,1000,2000,CLK_KHZ/freq_max,arr)
 * semantics. Actual MCPWM duty/deadtime/active-freewheel still require
 * the product-specific physical PWM backend.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t period_ticks;
    uint32_t freq_min_period_ticks;
    uint32_t freq_max_period_ticks;
    uint32_t nominal_frequency_hz; /* integer-rounded informational value */
} AM13E_MotorFrequencyPlan;
/* Return 1 iff params are within Rel17 configuration bounds and
 * representable by a 16-bit PWM timer. No registers are written.
 * ertm_us==0 means initial freq_min. 1000us selects freq_max; >=2000us
 * selects freq_min; interpolate PERIOD linearly in between.
 */
int am13e_motor_frequency_plan(uint32_t pwm_clock_hz,int freq_min_khz,
                               int freq_max_khz,int ertm_us,
                               AM13E_MotorFrequencyPlan *out);
