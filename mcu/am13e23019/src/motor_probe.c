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
    hw_motor_sine_update(8000, 1000, 2000, 4000);
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
