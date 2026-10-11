/* ESCape32 Rel17 UART telemetry mode selection, MCU-independent.
 * Physical UART pin mux, single-wire inversion, DMA and RX timeout
 * must be implemented separately against the AM13E23019 SDK/board.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t baud_rate;
    uint16_t rx_timeout_bits;
    uint8_t rx_protocol;
    uint8_t inverted_line;
    uint8_t requires_turnaround; /* input->reply; iBUS, S.Port, MSB, HoTT */
    uint8_t legacy_single_wire; /* Rel17 sets USART_CR3_HDSEL for ALL modes */
} AM13E_TelemModePlan;
/* Modes 0..6 preserve original src/telem.c timing/policy. */
int am13e_telem_mode_plan(int mode, AM13E_TelemModePlan *out);
/* AM13E230x UART RXTOSEL hardware is 4 bits, accepted register values
 * 0..15. Rel17 S.Port requires 26 bit-times, so it must use another
 * frame-gap mechanism. Reject instead of truncating to 10 or 15.
 */
int am13e_telem_rx_timeout_register(const AM13E_TelemModePlan *plan,
                                    uint8_t *field_value);
