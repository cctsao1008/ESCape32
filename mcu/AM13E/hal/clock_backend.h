/*
 * ESCape32 Rel17 AM13E23019 clock configuration contract.
 *
 * Product HW Architecture Baseline v1.6 reserves PC16_X1 and PC17_X2
 * for an external HFXT crystal, not HFCLK_IN. The 25MHz crystal
 * frequency is an accepted detailed clock decision beyond v1.6.
 *
 * Agreed CPU clock: XTAL 25MHz -> SYSPLL VCO 400MHz -> MCLK 200MHz.
 * See CLOCK_CONTRACT.md for the exact TI SDK field encoding and
 * mandatory Flash RWAIT=3, MCLK2=/2 and MCLK4=/4 constraints.
 *
 * Application uses the internal 32MHz SYSOSC inherited from Boot
 * until HFCLKGOOD and SYSPLLGOOD are confirmed, with PRIMASK set.
 * No MCPWM/power-stage output is configured or enabled here.
 *
 * This implementation is based on the LP crystal circuit; a customer
 * board must independently qualify its oscillator pins, load caps,
 * XTAL startup and clock measurements before hardware enable.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E23019 Application clock contract is target-specific"
#endif

#include <stdint.h>
#include "runtime_tick_contract.h"

#define AM13E_APP_XTAL_HZ  UINT32_C(25000000)
#define AM13E_APP_MCLK_HZ  UINT32_C(200000000)
#define AM13E_APP_VCO_HZ   UINT32_C(400000000)


/* Returns 200MHz only after oscillator/PLL/clock/flash status checks.
 * The returned frequency is computed from verified dividers, not an
 * independent physical frequency measurement. Returns 0 on failure.
 */
uint32_t am13e_app_clock_configure_xtal25(void);
