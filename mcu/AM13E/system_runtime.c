/*
 * ESCape32 Rel17 AM13E — external 25 MHz XTAL clock integration.
 *
 * XTAL=25 MHz is a BOARD REQUIREMENT, not the CPU clock rate.
 * Board-approved MCLK (XTAL direct or XTAL->SYSPLL) is still OPEN.
 *
 * This file implements the fail-closed clock/timebase handshake and
 * SysTick after a real clock backend has configured hardware. It
 * does not fabricate PLL settings, XTAL pinmux or oscillator timing.
 * Final ELF cannot link without am13e_app_clock_configure_xtal25().
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
 * The LaunchPad-reference 25 MHz XTAL must be qualified and brought up by a
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

    /* No synthetic MCLK defaults: reject invalid/unquantized rates
     * before committing to Rel17's 16 kHz SysTick scheduler.
     */
    const uint32_t cycles = hz / AM13E_APP_SYSTICK_HZ;
    if (hz == 0U ||
        hz % AM13E_APP_SYSTICK_HZ != 0U ||
        cycles < 2U ||
        cycles > AM13E_APP_SYSTICK_MAX_CYCLES ||
        DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_HSCLK ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_HFCLKGOOD_MASK) !=
        DL_SYSCTL_CLK_STATUS_HFCLK_GOOD) {
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
