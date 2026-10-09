/*
 * ESCape32 Rel17 AM13E — external 25 MHz XTAL clock integration.
 *
 * XTAL=25 MHz is a BOARD REQUIREMENT, not the CPU clock rate.
 * The agreed MCLK is 200MHz from SYSPLLCLK0 (400MHz VCO /2).
 *
 * This file implements the fail-closed clock/timebase handshake and
 * SysTick after a real clock backend has configured hardware. It
 * does not fabricate XTAL pinmux or oscillator electrical parameters.
 * The real clock implementation resides in clock_xtal25_pll200.c;
 * the final Application ELF remains blocked by other board backends.
 */
#include "common.h" /* init() declaration and TI CMSIS */
#include "motor_backend.h"
#include "clock_backend.h"
#include <dl_sysctl.h>
#include <dl_systick.h>

#define AM13E_APP_SYSTICK_HZ UINT32_C(16000)
#define AM13E_APP_SYSTICK_MAX_CYCLES UINT32_C(0x01000000)

static uint32_t app_mclk_hz;

/* Application init() runs with Boot PRIMASK still set and inherited
 * SYSOSC 32 MHz (temporary handoff clock; not Application policy).
 * The product's 25 MHz HFXT on PC16_X1/PC17_X2 must be brought up by a
 * real backend, which selects and verifies the final MCLK.
 */
void init(void)
{
    if (DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_SYSOSC ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) !=
        SYSCTL_CLKSTATUS_SYSOSCFREQ_SYSOSC32M) {
        for (;;) { __NOP(); }
    }

    const uint32_t hz = am13e_app_clock_configure_xtal25();

    /* Verify the agreed 200MHz PLL policy, not just any HSCLK.
     * Physical frequency measurement remains a separate hardware gate.
     */
    const uint32_t cycles = hz / AM13E_APP_SYSTICK_HZ;
    if (hz != AM13E_APP_MCLK_HZ ||
        hz % AM13E_APP_SYSTICK_HZ != 0U ||
        cycles < 2U ||
        cycles > AM13E_APP_SYSTICK_MAX_CYCLES ||
        DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_HSCLK ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_HFCLKGOOD_MASK) !=
        DL_SYSCTL_CLK_STATUS_HFCLK_GOOD ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) !=
        DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_HSCLKGOOD_MASK) !=
        DL_SYSCTL_CLK_STATUS_HSCLK_GOOD) {
        for (;;) { __NOP(); }
    }
    app_mclk_hz = hz;
}

/* The board must have called init() to establish and verify MCLK.
 * This function does not reprogram XTAL or change MCLK a second
 * time. Program 16 kHz SysTick using the verified CPU MCLK value.
 *
 * The TI SDK handles SysTick register configuration; PRIMASK remains
 * set until the separate, board-qualified safe-enable barrier runs.
 */
void am13e_app_motor_runtime_tick_init(void)
{
    if (app_mclk_hz == 0U) {
        for (;;) { __NOP(); }
    }

    DL_SYSTICK_init(app_mclk_hz / AM13E_APP_SYSTICK_HZ);
    NVIC_SetPriority(PendSV_IRQn, (1U << (__NVIC_PRIO_BITS - 1U)));
    NVIC_SetPriority(SysTick_IRQn, 0U);
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();
}
