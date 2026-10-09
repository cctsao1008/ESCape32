/*
 * AM13E Rel17 Application telemetry transport contract (Stage E1-B).
 *
 * Declaration only: this is NOT a peripheral driver and it deliberately
 * contains no fake-success implementation. Linking a real firmware must
 * fail until the device-specific USART/DMA/IRQ implementation is supplied.
 *
 * init: backend owns UART baud/inversion/half-duplex/DMA/pinmux for mode;
 * the RX buffer is loaned by Rel17 and remains live until reconfiguration.
 * on_rx_frame: backend fills init's RX buffer then calls it; result:
 *   >0 = reply bytes available in RX buffer; 0 = no immediate reply;
 *   <0 = protocol owns deferred output (HoTT).
 * on_tx_done: backend calls once for completed queued-buffer TX.
 * tx_start: asynchronous noncopying transmit. No concurrent re-use of
 * transmitted bytes before TX-complete notification.
 * tx_byte: transmit a HoTT delayed byte; last_byte requests completion
 * signaling/turnaround. Backend owns the physical UART behavior.
 *
 * No board UART, timer, clock, pinmux or polarity is assumed here.
 */
#pragma once

#if !defined(AM13E)
#error "AM13E telemetry interface is not for STM32/AT32/GD32 targets"
#endif

#include <stdbool.h>
#include <stdint.h>

void am13e_telem_hw_init(int mode, char *rx_buffer,
                         unsigned int capacity, bool rx_required);
bool am13e_telem_hw_tx_busy(void);
void am13e_telem_hw_tx_start(const char *bytes, unsigned int length);
void am13e_telem_hw_tx_byte(uint8_t byte, bool last_byte);
void am13e_telem_hw_pause_rx(void);

int am13e_telem_on_rx_frame(unsigned int received);
void am13e_telem_on_tx_done(void);
