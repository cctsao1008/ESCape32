/*
 * AM13E23019 MCPWM backend compile probe.
 *
 * This image is not a motor-running firmware.  It only type-checks and links
 * the first ESCape32 motor-backend operations against the pinned TI SDK.
 * TBCLK is never enabled, so this probe does not intentionally start PWM.
 */

#include "ti_sdk_dl_config.h"

#define ESCAPE32_AM13E_MCPWM_INST MCPWM_1_INST
#include "hw_motor_am13e.h"

static void am13e_motor_backend_compile_probe(void)
{
    hw_motor_am13e_configure_split_deadband(40);
    hw_motor_sine_update(8000, 1000, 2000, 4000);

    /*
     * Stage C: verify the exact AQ shadow API used by the TI global-load
     * example for all three phase pairs.  These values are compile probes
     * only; they are not yet the ESCape32 six-step gate-state mapping.
     */
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_1A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_1B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_2A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_2B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_3A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_3B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    /*
     * The SDK exposes pair-local continuous software force.  Compile-check it
     * here, but do not use AQSFRC for normal commutation: AQSFRC has no shadow
     * register, so multi-phase updates would not be atomic.  Six-step state
     * changes will be encoded in AQCTLA/B shadow tables instead.
     */
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_SW_FORCE_DISABLED);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_2B, DL_MCPWM_AQ_SW_FORCE_DISABLED);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_3A, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_3B, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);

    hw_motor_commit_update();
}

int main(void)
{
    /*
     * Keep the probe function compiled and type-checked without executing it.
     * The volatile condition prevents the compiler front-end from discarding
     * the function body before semantic/API checking.
     */
    volatile int run_probe = 0;

    if (run_probe) {
        am13e_motor_backend_compile_probe();
    }

    for (;;) {
    }
}
