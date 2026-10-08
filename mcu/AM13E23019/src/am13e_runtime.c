/*
 * E62 AM13E application bring-up, not the complete ESCape32 rel17 firmware.
 *
 * This is an executable MCPWM0 six-step vertical slice using the real
 * ESCape32 AM13E hardware backend. The board-specific PWM pinmux,
 * protection configuration and gate-driver enable still need integration.
 * No code in this file asserts the unknown PB13 enable polarity.
 */
#include <stdint.h>
#include "ti_sdk_dl_config.h"
#include "am13e_bemf_events.h"
#include "am13e_timg12.h"
#include "am13e_irq.h"


#define ESCAPE32_AM13E_MCPWM_INST MCPWM0
#include "hw_motor_am13e.h"

static const uint8_t am13e_positive[6] = {4U, 1U, 1U, 2U, 2U, 4U};
static const uint8_t am13e_negative[6] = {2U, 2U, 4U, 4U, 1U, 1U};

volatile uint32_t am13e_debug_step;
volatile uint32_t am13e_debug_enable;
volatile uint32_t am13e_debug_bemf_interval;
volatile uint32_t am13e_debug_bemf_ready; /* must remain 0 until TIMG initialized */
volatile uint32_t am13e_debug_command_seq; /* increment after updating step/enable */

static void am13e_next_commutation(void)
{
    uint32_t step = am13e_debug_step;
    step = (step < 1U || step >= 6U) ? 1U : step + 1U;
    am13e_debug_step = step;
    if (am13e_debug_bemf_ready == 1U) am13e_bemf_ecap_start_epoch();
    (void)hw_motor_am13e_runtime_commutate(
        am13e_positive[step - 1U], am13e_negative[step - 1U], true);
}


int main(void)
{
    SYSCFG_DL_init();
    am13e_timg12_init();
    if (!am13e_register_bemf_irqs()) {
        for (;;) { __WFI(); }
    }

    /* Start actual MCPWM0 timebase and AQ carriers. Initial output is FLOAT.
     * Duty, dead-time and PWM clock must be reconciled with E62 product spec.
     * This value is a bring-up placeholder, not calibrated ESCape32 timing.
     */
    if (!hw_motor_am13e_runtime_start(8000U, 40U, 2000U)) {
        for (;;) { __WFI(); }
    }

    /*
     * Debugger-accessible commands: no autonomous commutation without BEMF.
     * Set enable=1 and step=1..6 via debugger to request a commutation state;
     * enable=0 returns to all-phase FLOAT. Keeps the actual MCPWM register
     * control path exercisable without replacing the ESCape32 control loop.
     */
    (void)am13e_bemf_event_setup(am13e_next_commutation, 1000U, 100000000U, 0U);
    uint32_t last_command_seq = 0U;
    for (;;) {
        /* Debug-only event injection; future eCAP ISR will supply interval. */
        if (am13e_debug_bemf_ready == 1U && am13e_debug_bemf_interval != 0U) {
            uint32_t interval = am13e_debug_bemf_interval;
            am13e_debug_bemf_interval = 0U;
            (void)am13e_bemf_event_capture_interval(interval);
        }
        if (am13e_debug_bemf_ready == 1U) {
            /* Poll eCAP0 until E62 IRQ registration is configured.
             * CAP1 epoch must be tied to previous commutation boundary.
             */
            (void)am13e_bemf_event_ecap0_event1();
            am13e_bemf_event_timg12_irq();
        }
        uint32_t command_seq = am13e_debug_command_seq;
        if (command_seq != last_command_seq) {
            uint32_t enable = am13e_debug_enable;
            uint32_t step = am13e_debug_step;
            if (enable == 1U && step >= 1U && step <= 6U) {
                if (am13e_debug_bemf_ready == 1U) am13e_bemf_ecap_start_epoch();
                (void)hw_motor_am13e_runtime_commutate(
                    am13e_positive[step - 1U], am13e_negative[step - 1U], true);
            } else {
                (void)hw_motor_am13e_runtime_commutate(0U, 0U, false);
            }
            last_command_seq = command_seq;
        }
    }
}
