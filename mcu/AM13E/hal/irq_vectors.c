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
#include "serial_uart_rx.h"
#ifdef AM13E_BOARD_TELEM_UART_CONFIGURED
#include "telem_backend.h"
#endif
#include "motor_bemf.h"
#include <dl_gpio.h>

void SysTick_Handler(void)
{
    /* PB15 is IO-only: no sampling, IRQ or driver fault processing.
     * PA26 is serviced by the TIMG4 capture/DMA RX backend.
     */
    sys_tick_handler();
#ifdef AM13E_BOARD_TELEM_UART_CONFIGURED
    am13e_telem_hw_systick();
#endif
    /* PA26 capture DMA prefix / RX-only frame-gap processing. */
    am13e_app_pb14_systick();
    /* BEMF ECAP1 timeouts must be bounded even if no edge arrives. */
    am13e_app_motor_bemf_tick();
}

void PendSV_Handler(void)
{
    pend_sv_handler();
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
    /* EXBUS bitrate switch only when UART RX is genuinely idle.
     * Never alter UNICOMM clock/dividers inside UART or SysTick IRQ.
     */
    am13e_app_serial_exbus_service_pending();
#endif
    /* DShot CMD_SAVE is queued by PA26 TIMG4/DMA IRQ, not executed there.
     * PendSV can be preempted by higher-priority valid RX, preserving
     * WWDT feeding between individual Flash commands.
     */
    am13e_app_io_service_pending_save();
#if defined(AM13E_BOARD_SERIAL_RX_CONFIGURED) && \
    defined(AM13E_BOARD_SERIAL_TX_CONFIGURED)
    /* CRSF Write/Save/Reset executes in preemptible PendSV.
     * UC RX IRQ only validates/copies the command frame.
     */
    am13e_app_serial_transport_service_pending();
    am13e_app_serial_crsf_service_pending();
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    am13e_app_serial_cli_service_pending();
#endif
#endif
}

void HardFault_Handler(void)
{
    hard_fault_handler();
}

/* GPIO1 vector remains for future non-PB15 clients. In this IO-only
 * reference build PB15 interrupts are NOT enabled and no nFAULT
 * handler/Trip exists. PA26/DShot uses TIMG4/DMA, not this IRQ.
 */
void GPIO1_IRQHandler(void)
{
    const uint32_t pending = DL_GPIO_getEnabledInterruptStatus(GPIO1,
                                                               UINT32_MAX);
    /* PB15 interrupts are disabled by initgpio(). A spurious or
     * misconfigured interrupt must not be interpreted as nFAULT logic.
     */
    /* Do not silently discard unexpected GPIO1 events.
     * Their real backend MUST acknowledge and handle assigned sources.
     * Intentionally unresolved until input capture has been ported.
     */
    if (pending != 0U) {
        am13e_app_io_on_gpio1_interrupt(pending);
    }
}
