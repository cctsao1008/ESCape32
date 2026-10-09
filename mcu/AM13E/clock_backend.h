/*
 * AM13E ESCape32 Application external XTAL clock requirement.
 *
 * The application hardware reference is a 25 MHz CRYSTAL on X1/X2,
 * based on LP-AM13E230 LaunchPad Y1 (SLVUDH9, section 2.4).
 * NOT an 8 MHz single-ended digital oscillator on HFCLK_IN.
 *
 * This is declarations only. No PLL multiplier, MCLK rate, XTAL
 * startup time, load capacitance or voltage scaling is invented.
 *
 * On entry from Boot, MCLK is reset-default SYSOSC 32 MHz and
 * PRIMASK is set. The actual implementation MUST:
 *
 * 1. Qualify the physical X1/X2 crystal and its load capacitors,
 *    verify 25 MHz XTAL drive and load for the actual AM13E board.
 * 2. Set up XTAL pad/IOMUX and oscillator startup using real TI SDK.
 * 3. Confirm HFCLKGOOD with a bounded failure path (never switch
 *    the CPU clock to an unstable oscillator).
 * 4. Optionally configure SYSPLL with HFCLK=25 MHz as reference,
 *    using an explicit, board-approved target MCLK frequency.
 * 5. Switch MCLK to HSCLK and verify the *actual* source/rate.
 * 6. Return that verified MCLK rate (Hz); no guessed nominal value.
 * 7. Preserve disabled power-stage outputs and PRIMASK state.
 *
 * A 25 MHz XTAL reference does NOT imply a 25 MHz CPU clock.
 * No real implementation is provided: final firmware linking
 * intentionally fails until the XTAL+MCLK backend is supplied.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E Application 25 MHz XTAL clock contract is target-specific"
#endif

#include <stdint.h>
#define AM13E_APP_XTAL_HZ UINT32_C(25000000)

/* Return 0 on failure, or a validated current CPU MCLK in Hz. */
uint32_t am13e_app_clock_configure_xtal25(void);
