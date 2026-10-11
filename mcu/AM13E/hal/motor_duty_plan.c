#include "motor_duty_plan.h"
#include <stddef.h>
#include <stdint.h>
int am13e_motor_duty_plan(uint32_t period_ticks,uint32_t pwm_clock_hz,
                          int logical_duty,int dead_ticks,
                          int lock,int running,int damp,int brushed,
                          int full_duty,AM13E_MotorDutyPlan *out)
{
    if (out == NULL || period_ticks < 2U || period_ticks > UINT16_MAX ||
        pwm_clock_hz == 0U || pwm_clock_hz % UINT32_C(1000000) != 0U ||
        dead_ticks < 0 || (uint32_t)dead_ticks > period_ticks) return 0;

    const uint32_t upper = (full_duty != 0) ? period_ticks :
        (brushed != 0) ? period_ticks -
            ((pwm_clock_hz / UINT32_C(1000000)) * 3U / 2U) : period_ticks;
    /* Check in advance: unsigned subtraction for a too-short period
     * must not wrap to a large, superficially valid compare count.
     */
    if (full_duty == 0 && brushed != 0 &&
        (pwm_clock_hz / UINT32_C(1000000)) * 3U / 2U >= period_ticks)
        return 0;
    const uint32_t lower = ((lock != 0) || (running != 0 && damp != 0)) ?
                               (uint32_t)dead_ticks : 0U;
    if (lower > upper) return 0;

    const uint32_t bounded = (logical_duty < 0) ? 0U :
                             (logical_duty > 2000) ? 2000U :
                             (uint32_t)logical_duty;
    const uint32_t compare = lower +
        (uint32_t)(((uint64_t)bounded * (upper - lower)) / UINT64_C(2000));
    const AM13E_MotorDutyPlan plan = {
        .period_register_ticks = (uint16_t)(period_ticks - (full_duty != 0)),
        .compare_ticks = (uint16_t)compare,
        .lower_compare_ticks = (uint16_t)lower,
        .upper_compare_ticks = (uint16_t)upper
    };
    *out = plan;
    return 1;
}
