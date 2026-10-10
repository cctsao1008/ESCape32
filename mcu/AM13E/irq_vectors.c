/*
 * TI AM13E Cortex-M33 exception vector bridge for ESCape32 Rel17.
 *
 * The actual FW1 main(), tick/housekeeping and fault policy remain in
 * src/main.c. This file adapts the TI startup CMSIS vector names only.
 */
#include "irq_vectors.h"
#include "fault_input.h"
#include "io_backend.h"
#include "command_capture.h"
#include "motor_bemf.h"
#include <dl_gpio.h>

void SysTick_Handler(void)
{
    /* PB15 is IO-only: no sampling, IRQ or driver fault processing.
     * PB14 is serviced by its independent ECAP0 RX backend.
     */
    sys_tick_handler();
    /* PB14 ECAP reference calibration / RX-only frame-gap processing. */
    am13e_app_pb14_systick();
    /* BEMF ECAP1 timeouts must be bounded even if no edge arrives. */
    am13e_app_motor_bemf_tick();
}

void PendSV_Handler(void)
{
    pend_sv_handler();
    /* DShot CMD_SAVE is queued by PB14/ECAP0 IRQ, not executed there.
     * PendSV can be preempted by higher-priority valid RX, preserving
     * WWDT feeding between individual Flash commands.
     */
    am13e_app_io_service_pending_save();
}

void HardFault_Handler(void)
{
    hard_fault_handler();
}

/* GPIO1 vector remains for future non-PB15 clients. In this IO-only
 * reference build PB15 interrupts are NOT enabled and no nFAULT
 * handler/Trip exists. PB14/DShot uses ECAP0, not this IRQ.
 */
void GPIO1_IRQHandler(void)
{
    const uint32_t pending = DL_GPIO_getEnabledInterruptStatus(GPIO1,
                                                               UINT32_MAX);
    /* PB15 interrupts are disabled by initgpio(). A spurious or
     * misconfigured interrupt must not be interpreted as nFAULT logic.
     */
    /* Do not silently discard future PB14 DShot/command GPIO events.
     * Their real backend MUST acknowledge and handle assigned sources.
     * Intentionally unresolved until input capture has been ported.
     */
    if (pending != 0U) {
        am13e_app_io_on_gpio1_interrupt(pending);
    }
}
