/* Board-selected TI UNICOMM UART RX provider, original Rel17 input.
 * Compiled only for an explicitly assigned real board UART.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E UART RX provider is target-specific"
#endif
#if defined(AM13E_BOARD_SBUS2_TIMED_REPLY) && !defined(AM13E_BOARD_SERIAL_RX_CONFIGURED)
#error "Timed SBUS2 requires the native board-selected serial receiver"
#endif
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#include <stdbool.h>
bool am13e_app_serial_rx_init(unsigned mode);
bool am13e_app_serial_rx_irq_ready(void);
/* Called from existing 16kHz SysTick only: no new motor timer. */
void am13e_app_serial_rx_systick(void);
/* PendSV-only EXBUS autobaud. No blocking UART Busy wait in RX IRQ. */
void am13e_app_serial_exbus_service_pending(void);
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
/* PendSV-only, never from UART RX ISR. Original CRSF command executor
 * may perform Flash operations and requires preemptible context.
 */
void am13e_app_serial_crsf_service_pending(void);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
/* Original HIGH timeout entry before receiver selection. Board selection
 * qualifies the existing PB14 entry input and separate UART TX/RX pads.
 * Requires PRIMASK=1 and an idle full-duplex UART and TX pin.
 */
bool am13e_app_serial_cli_enter(void);
void am13e_app_serial_cli_entry_poll(int high,int edge,int selected);
void am13e_app_serial_cli_service_pending(void);
#endif
#endif
#endif

#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
void am13e_app_serial_sbus2_timer_irq(void);
#endif

/* Lowest-priority UART transport recovery; no protocol parser/state policy. */
void am13e_app_serial_transport_service_pending(void);

#ifdef AM13E_BOARD_DEBUG_UART_BAUD
#include <stdbool.h>
#include <stdint.h>
void am13e_app_debug_uart_init(void);
bool am13e_app_debug_uart_try_write(uint8_t byte);
bool am13e_app_debug_uart_try_read(uint8_t *byte);
#endif
