/*
 * TI AM13E Cortex-M33 exception vector bridge for ESCape32 Rel17.
 *
 * The actual FW1 main(), tick/housekeeping and fault policy remain in
 * src/main.c. This file adapts the TI startup CMSIS vector names only.
 */
#include "irq_vectors.h"
#include "gpio_runtime.h"

void SysTick_Handler(void)
{
    /* PB15 nFAULT belongs to E62 HW Baseline v1.6. A 16kHz software
     * observation is secondary supervision, NOT the MCPWM hardware trip.
     * Reject an asserted or uninitialized input before the Rel17 tick.
     * hard_fault_handler() requires the real motor shutdown/reset backend;
     * it must never be replaced with a fake-success callback.
     *
     * PB14 is NOT serviced here and GPIO1_IRQn is NOT claimed: the real
     * input-capture / DShot IRQ ownership remains a separate contract.
     */
    if (am13e_app_nfault_asserted()) {
        hard_fault_handler();
        for (;;) {} /* Fail closed if the fatal handler unexpectedly returns. */
    }
    sys_tick_handler();
}

void PendSV_Handler(void)
{
    pend_sv_handler();
}

void HardFault_Handler(void)
{
    hard_fault_handler();
}
