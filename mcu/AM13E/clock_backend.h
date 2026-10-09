/*
 * ESCape32 Rel17 AM13E23019 clock configuration contract.
 *
 * Board reference: TI LP-AM13E230 LaunchPad Y1, 25MHz quartz crystal
 * connected across X1 (PC16) / X2 (PC17), not HFCLK_IN.
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

#define AM13E_APP_XTAL_HZ  UINT32_C(25000000)
#define AM13E_APP_MCLK_HZ  UINT32_C(200000000)
#define AM13E_APP_VCO_HZ   UINT32_C(400000000)

/* Returns 200MHz only after oscillator/PLL/clock/flash status checks.
 * The returned frequency is computed from verified dividers, not an
 * independent physical frequency measurement. Returns 0 on failure.
 */
uint32_t am13e_app_clock_configure_xtal25(void);
