/*
** AM13E23019 ESCape32 motor backend -- incremental bring-up.
**
** Stage B currently validates only the MCPWM shadow-update / global-load path.
** Six-step output-state control is intentionally not implemented yet; those
** operations must not be mapped until the AQ/dead-band semantics are proven.
**
** ESCAPE32_AM13E_MCPWM_INST must resolve to the SysConfig-generated MCPWM
** instance used by the build target.
*/

#pragma once

#include <stdint.h>
#include "dl_mcpwm.h"

#ifndef ESCAPE32_AM13E_MCPWM_INST
#error "Define ESCAPE32_AM13E_MCPWM_INST to the SysConfig MCPWM instance"
#endif

static inline __attribute__((always_inline))
void hw_motor_sine_update(int period, int phase_a, int phase_b, int phase_c)
{
    /*
     * TI's MCPWM global-load example updates the 1A/2A/3A compare shadow
     * registers, then arms a one-shot global load.  Keep exactly that model
     * here.  The B outputs/dead-band relationship is configured separately
     * and is not asserted by this Stage-B backend.
     */
    DL_MCPWM_setTimeBasePeriodShadow(
        ESCAPE32_AM13E_MCPWM_INST, (uint16_t)period);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_1A,
        (uint16_t)phase_a);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_2A,
        (uint16_t)phase_b);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_3A,
        (uint16_t)phase_c);
}

static inline __attribute__((always_inline))
void hw_motor_commit_update(void)
{
    DL_MCPWM_setGlobalLoadOneShotLatch(ESCAPE32_AM13E_MCPWM_INST);
}

/*
 * Deliberate compile-time stops for semantics that are not validated yet.
 * These macros only fire when a caller tries to use the unfinished operation.
 */
#define hw_motor_sine_enable_outputs()                                      \
    do {                                                                    \
        _Static_assert(0,                                                   \
            "AM13E sine output-enable semantics not validated yet");        \
    } while (0)

#define hw_motor_apply_six_step(positive, negative, damp)                   \
    do {                                                                    \
        (void)(positive);                                                   \
        (void)(negative);                                                   \
        (void)(damp);                                                       \
        _Static_assert(0,                                                   \
            "AM13E six-step AQ/dead-band mapping not validated yet");       \
    } while (0)

#define hw_motor_set_idle_pwm_mode()                                        \
    do {                                                                    \
        _Static_assert(0,                                                   \
            "AM13E idle PWM output-state mapping not validated yet");       \
    } while (0)
