/* Board-selected TI UNICOMM UART RX provider, original Rel17 input.
 * Compiled only for an explicitly assigned real board UART.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E UART RX provider is target-specific"
#endif
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#include <stdbool.h>
bool am13e_app_serial_rx_init(unsigned mode);
bool am13e_app_serial_rx_irq_ready(void);
#endif
