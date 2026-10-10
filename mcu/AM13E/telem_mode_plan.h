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
    uint8_t requires_turnaround;
} AM13E_TelemModePlan;
/* Modes 0..6 preserve original src/telem.c timing/policy. */
int am13e_telem_mode_plan(int mode, AM13E_TelemModePlan *out);
