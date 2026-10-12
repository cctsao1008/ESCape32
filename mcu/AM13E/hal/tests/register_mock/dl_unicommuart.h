#pragma once
#include AM13E_TEST_TI_UART_HEADER
/* Model only the SDK IIDX read's acknowledge side effect. */
extern unsigned fixture_uart_rx_pending;
static inline DL_UART_IIDX test_uart_iidx(DL_UNICOMM_Inst_Regs *u)
{
    if(fixture_uart_rx_pending==2U){
        *(volatile uint32_t *)(uintptr_t)&u->uart->STAT|=UNICOMMUART_STAT_RXFE_SET;fixture_uart_rx_pending=0U;
    }else if(fixture_uart_rx_pending==1U)fixture_uart_rx_pending=2U;
    DL_UART_IIDX i=DL_UART_getPendingInterrupt(u);
    *(volatile uint32_t *)(uintptr_t)&u->uart->CPU_INT.IIDX=0U;
    return i;
}
#define DL_UART_getPendingInterrupt test_uart_iidx
