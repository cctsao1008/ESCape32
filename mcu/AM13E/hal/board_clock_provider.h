/*
 * AM13E board-provided clock startup interface.
 * The selected board MUST verify the actual SYSCTL/FRI settings and
 * its declared MCLK before returning. No weak no-op or guessed default.
 * Returns zero on configuration/readback failure. It must not unmask IRQ
 * or touch a gate/motor output.
 */
#pragma once
#ifndef AM13E
#error "AM13E board clock provider required"
#endif
#include <stdint.h>
uint32_t am13e_board_clock_start(void);
