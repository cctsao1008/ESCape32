#pragma once
#include <stdint.h>
#include "bidir_codec.h"
#define AM13E_BIDIR_DMA_TRANSFERS AM13E_BIDIR_DMA_LEVELS
#define AM13E_BIDIR_TURNAROUND_US 30U
#define AM13E_BIDIR_MIN_DELAY_TICKS 16U

/* Pure capture-clock -> timer-clock turnaround planner.
 * Final edge is the actual DShot RX trailing edge; counter arithmetic
 * is modulo 2^32 to tolerate ECAP TSCTR rollover. 0 means reject:
 * invalid clock, already past the 30us deadline or <16 TX timer ticks.
 * This is timing-source correctness, NOT validated IRQ/DMA latency.
 */
uint32_t am13e_bidir_turnaround_ticks(uint32_t final_edge,
                                     uint32_t counter_now,
                                     uint32_t capture_hz,
                                     uint32_t timer_hz);
uint32_t am13e_bidir_tx_period_ticks(uint32_t rx_ticks,uint32_t rx_hz,uint32_t timer_hz);
void am13e_bidir_toggle_plan(const uint8_t levels[AM13E_BIDIR_DMA_LEVELS],
                            uint32_t pin_mask,uint32_t words[AM13E_BIDIR_DMA_TRANSFERS]);
