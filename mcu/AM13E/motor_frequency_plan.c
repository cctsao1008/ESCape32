#include "motor_frequency_plan.h"
#include <stddef.h>
#include <stdint.h>
#define MAX_PERIOD_TICKS UINT32_C(65535)

int am13e_motor_frequency_plan(uint32_t pwm_clock_hz,int freq_min_khz,
                               int freq_max_khz,int ertm_us,
                               AM13E_MotorFrequencyPlan *out)
{
    if (out == NULL || pwm_clock_hz == 0U || ertm_us < 0 ||
        freq_min_khz < 16 || freq_min_khz > 48 ||
        freq_max_khz < 16 || freq_max_khz > 96 ||
        freq_max_khz < freq_min_khz) return 0;

    /* Rel17 initial: arr = CLK_KHZ / cfg.freq_min.
     * Fixed rounding follows C integer division, not a continuous
     * interpolation of 1/frequency or a second frequency ramp.
     */
    const uint64_t clock_khz=(uint64_t)pwm_clock_hz/UINT64_C(1000);
    const uint64_t period_min=clock_khz/(uint32_t)freq_min_khz;
    const uint64_t period_max=clock_khz/(uint32_t)freq_max_khz;
    if (period_min < 2U || period_min > MAX_PERIOD_TICKS ||
        period_max < 2U || period_max > MAX_PERIOD_TICKS) return 0;

    uint64_t selected = period_min;
    if (ertm_us != 0) {
        if (ertm_us <= 1000) selected = period_max;
        else if (ertm_us >= 2000) selected = period_min;
        else selected = period_max +
            ((uint64_t)(ertm_us-1000)*(period_min-period_max))/UINT64_C(1000);
    }
    AM13E_MotorFrequencyPlan plan={
        .period_ticks=(uint32_t)selected,
        .freq_min_period_ticks=(uint32_t)period_min,
        .freq_max_period_ticks=(uint32_t)period_max,
        .nominal_frequency_hz=(uint32_t)(((uint64_t)pwm_clock_hz+selected/2U)/selected)
    };
    *out=plan;
    return 1;
}
