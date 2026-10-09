/*
 * TI AM13E Cortex-M33 exception vector bridge for ESCape32 Rel17.
 *
 * The actual FW1 main(), tick/housekeeping and fault policy remain in
 * src/main.c. This file adapts the TI startup CMSIS vector names only.
 */
#include "irq_vectors.h"
#include "gpio_runtime.h"
#include "io_backend.h"
#include "pb14_capture.h"
#include <dl_gpio.h>

void SysTick_Handler(void)
{
    /* PB15 nFAULT belongs to E62 HW Baseline v1.6. A 16kHz software
     * observation is secondary supervision, NOT the MCPWM hardware trip.
     * Reject an asserted or uninitialized input before the Rel17 tick.
     * hard_fault_handler() requires the real motor shutdown/reset backend;
     * it must never be replaced with a fake-success callback.
     *
     * PB14 is NOT serviced in the SysTick path. GPIO1 has a shared
     * real IRQ vector below; PB14 events require an input backend.
     */
    if (am13e_app_nfault_asserted()) {
        hard_fault_handler();
        for (;;) {} /* Fail closed if the fatal handler unexpectedly returns. */
    }
    sys_tick_handler();
    /* PB14 ECAP reference calibration / RX-only frame-gap processing. */
    am13e_app_pb14_systick();
}

void PendSV_Handler(void)
{
    pend_sv_handler();
}

void HardFault_Handler(void)
{
    hard_fault_handler();
}

/* Strong TI CMSIS vector: GPIO1 is shared by PB15 and any PB14 GPIO
 * events. The original ESCape32 control policy remains in src/main.c.
 * Hardware MCPWM Trip-Zone (not implemented here) is required for
 * bounded-latency protection of the power stage.
 */
void GPIO1_IRQHandler(void)
{
    const uint32_t pending = DL_GPIO_getEnabledInterruptStatus(GPIO1,
                                                               UINT32_MAX);
    if ((pending & AM13E_APP_NFAULT_PIN_MASK) != 0U) {
        /* Acknowledge ONLY PB15, never another client's interrupt. */
        DL_GPIO_clearInterruptStatus(GPIO1, AM13E_APP_NFAULT_PIN_MASK);
        __disable_irq();
        hard_fault_handler();
        for (;;) { __NOP(); }
    }
    /* Do not silently discard future PB14 DShot/command GPIO events.
     * Their real backend MUST acknowledge and handle assigned sources.
     * Intentionally unresolved until input capture has been ported.
     */
    if (pending != 0U) {
        am13e_app_io_on_gpio1_interrupt(pending);
    }
}
