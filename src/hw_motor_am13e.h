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
        DL_MCPWM_COUNTER_COMPARE_1B,
        (uint16_t)phase_a);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_2A,
        (uint16_t)phase_b);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_2B,
        (uint16_t)phase_b);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_3A,
        (uint16_t)phase_c);
    DL_MCPWM_setCounterCompareShadowValue(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_COUNTER_COMPARE_3B,
        (uint16_t)phase_c);
}

/*
 * Configure a fixed split-input dead-band path:
 *
 *   OutA = RED(PWMA), active high
 *   OutB = NOT(FED(PWMB))
 *
 * With independent AQ inputs this supports, in principle:
 *   PWM/PWM   -> complementary pair with dead time
 *   PWM/HIGH  -> high-side PWM, low-side off
 *   LOW/LOW   -> high-side off, low-side on
 *   LOW/HIGH  -> both off (floating phase)
 *
 * DBCTL/DBRED/DBFED are module-wide on AM13E, so this configuration is shared
 * by all three phase pairs.  The per-phase state is carried by the pair-local
 * AQ A/B shadow registers, not by changing DBCTL during commutation.
 */
static inline __attribute__((always_inline))
void hw_motor_am13e_configure_split_deadband(uint16_t dead_time_ticks)
{
    DL_MCPWM_setDeadBandOutputSwapMode(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_OUTPUT_A, false);
    DL_MCPWM_setDeadBandOutputSwapMode(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_OUTPUT_B, false);

    DL_MCPWM_setDeadBandDelayMode(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_RED, true);
    DL_MCPWM_setDeadBandDelayMode(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_FED, true);

    DL_MCPWM_setDeadBandDelayPolarity(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_DB_RED,
        DL_MCPWM_DB_POLARITY_ACTIVE_HIGH);
    DL_MCPWM_setDeadBandDelayPolarity(
        ESCAPE32_AM13E_MCPWM_INST,
        DL_MCPWM_DB_FED,
        DL_MCPWM_DB_POLARITY_ACTIVE_LOW);

    DL_MCPWM_setRisingEdgeDeadBandDelayInput(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_INPUT_PWMA);
    DL_MCPWM_setFallingEdgeDeadBandDelayInput(
        ESCAPE32_AM13E_MCPWM_INST, DL_MCPWM_DB_INPUT_PWMB);

    DL_MCPWM_setRisingEdgeDelayCountActive(
        ESCAPE32_AM13E_MCPWM_INST, dead_time_ticks);
    DL_MCPWM_setRisingEdgeDelayCountShadow(
        ESCAPE32_AM13E_MCPWM_INST, dead_time_ticks);
    DL_MCPWM_setFallingEdgeDelayCountActive(
        ESCAPE32_AM13E_MCPWM_INST, dead_time_ticks);
    DL_MCPWM_setFallingEdgeDelayCountShadow(
        ESCAPE32_AM13E_MCPWM_INST, dead_time_ticks);
}

/*
 * AQ shadow primitives used to encode six-step phase states.
 *
 * CompleteShadow(..., 0) first clears every event action so a phase never
 * inherits stale compare/period behavior from its previous commutation state.
 * The encoder only writes shadow AQ registers; it does not enable TBCLK,
 * enable outputs, or arm a global-load event.
 */
static inline __attribute__((always_inline))
void hw_motor_am13e_aq_clear_shadow(
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE output)
{
    DL_MCPWM_setActionQualifierActionCompleteShadow(
        ESCAPE32_AM13E_MCPWM_INST, output, 0U);
}

static inline __attribute__((always_inline))
void hw_motor_am13e_aq_constant(
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE output,
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT level)
{
    hw_motor_am13e_aq_clear_shadow(output);

    /*
     * Program both cycle-boundary events.  This makes the intended constant
     * state explicit in the AQ table while leaving compare events inactive.
     */
    DL_MCPWM_setActionQualifierActionShadow(
        ESCAPE32_AM13E_MCPWM_INST,
        output,
        level,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    DL_MCPWM_setActionQualifierActionShadow(
        ESCAPE32_AM13E_MCPWM_INST,
        output,
        level,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
}

static inline __attribute__((always_inline))
void hw_motor_am13e_aq_pwm(
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE output,
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_EVENT compare_event)
{
    hw_motor_am13e_aq_clear_shadow(output);

    /* Edge-aligned source waveform: HIGH at ZERO, LOW at compare. */
    DL_MCPWM_setActionQualifierActionShadow(
        ESCAPE32_AM13E_MCPWM_INST,
        output,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    DL_MCPWM_setActionQualifierActionShadow(
        ESCAPE32_AM13E_MCPWM_INST,
        output,
        DL_MCPWM_AQ_OUTPUT_LOW,
        compare_event);
}

static inline __attribute__((always_inline))
void hw_motor_am13e_encode_phase_pair_shadow(
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE output_a,
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE output_b,
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_EVENT compare_a,
    DL_MCPWM_ACTION_QUALIFIER_OUTPUT_EVENT compare_b,
    bool positive,
    bool negative,
    bool damp)
{
    if (positive) {
        hw_motor_am13e_aq_pwm(output_a, compare_a);

        if (damp) {
            /* Same source waveform; dead-band makes final B complementary. */
            hw_motor_am13e_aq_pwm(output_b, compare_b);
        } else {
            /* B=HIGH before the inverted FED path => final low-side OFF. */
            hw_motor_am13e_aq_constant(
                output_b, DL_MCPWM_AQ_OUTPUT_HIGH);
        }
    } else if (negative) {
        /*
         * A=LOW and B=LOW before DB:
         * final A=LOW, final B=HIGH => low-side continuously ON.
         */
        hw_motor_am13e_aq_constant(
            output_a, DL_MCPWM_AQ_OUTPUT_LOW);
        hw_motor_am13e_aq_constant(
            output_b, DL_MCPWM_AQ_OUTPUT_LOW);
    } else {
        /*
         * A=LOW and B=HIGH before DB:
         * final A=LOW, final B=LOW => floating phase.
         */
        hw_motor_am13e_aq_constant(
            output_a, DL_MCPWM_AQ_OUTPUT_LOW);
        hw_motor_am13e_aq_constant(
            output_b, DL_MCPWM_AQ_OUTPUT_HIGH);
    }
}

static inline __attribute__((always_inline))
void hw_motor_am13e_encode_six_step_shadow(
    unsigned int positive_mask,
    unsigned int negative_mask,
    bool damp)
{
    hw_motor_am13e_encode_phase_pair_shadow(
        DL_MCPWM_AQ_OUTPUT_1A,
        DL_MCPWM_AQ_OUTPUT_1B,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB,
        (positive_mask & 0x1U) != 0U,
        (negative_mask & 0x1U) != 0U,
        damp);

    hw_motor_am13e_encode_phase_pair_shadow(
        DL_MCPWM_AQ_OUTPUT_2A,
        DL_MCPWM_AQ_OUTPUT_2B,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB,
        (positive_mask & 0x2U) != 0U,
        (negative_mask & 0x2U) != 0U,
        damp);

    hw_motor_am13e_encode_phase_pair_shadow(
        DL_MCPWM_AQ_OUTPUT_3A,
        DL_MCPWM_AQ_OUTPUT_3B,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB,
        (positive_mask & 0x4U) != 0U,
        (negative_mask & 0x4U) != 0U,
        damp);
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
