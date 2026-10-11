#include "motor_pwm_shadow_plan.h"
#include "motor_frequency_plan.h"
#include "motor_duty_plan.h"
#include <stddef.h>
int am13e_motor_pwm_shadow_plan(const AM13E_MotorPwmShadowInputs *input,
                                AM13E_MotorShadowPlan *out)
{
    if (input == NULL || out == NULL ||
        (input->full_duty != 0 && input->full_duty != 1)) return 0;
    AM13E_MotorFrequencyPlan frequency;
    if (!am13e_motor_frequency_plan(input->clock_hz,
                                    input->freq_min_khz,
                                    input->freq_max_khz,
                                    input->ertm_us,&frequency)) return 0;
    AM13E_MotorDutyPlan duty;
    if (!am13e_motor_duty_plan(frequency.period_ticks,input->clock_hz,
                               input->logical_duty,input->board_dead_ticks,
                               input->lock,input->running,input->damp,
                               input->brushed,input->full_duty,&duty)) return 0;
    /* At this stage all six compares are staged symmetrically. These are
     * inert register images, NOT actual complementary six-step waveforms.
     * Phase/AQ/dead-band ownership and the physical Trip Zone are separate.
     */
    AM13E_MotorShadowPlan p={.period=duty.period_register_ticks};
    for(unsigned i=0U;i<6U;++i)p.compare[i]=duty.compare_ticks;
    if (!am13e_motor_shadow_validate(&p)) return 0;
    *out=p;
    return 1;
}
