/*
 * ESCape32 Rel17 AM13E — baseline clock and SysTick runtime.
 *
 * This deliberately preserves the boot service's reset-default
 * SYSOSC 32 MHz MCLK as a FIRST integration baseline. It does not
 * configure SYSPLL, external XTAL, motor gates or board pinmux.
 *
 * Required: 16kHz SysTick -> Rel17 SysTick_Handler -> PendSV.
 * At 32MHz CPU clock, one tick is 2000 MCLK cycles. The backend
 * validates the actual TI SYSCTL clock state and halts closed if
 * the inherited clock does not satisfy this contract.
 *
 * Neither function unmasks PRIMASK. The separate board-validated
 * am13e_app_motor_runtime_enable_interrupts() must do so only
 * after safe outputs, fault protections and vectors are confirmed.
 *
 * This is NOT a full motor-ready clock tree; later board hardware
 * may need its own PLL/MCPWM clock policy and a measured tick rate.
 */
#include "motor_backend.h"
#include <soc.h>
#include <dl_sysctl.h>
#include <dl_systick.h>

#define AM13E_APP_BASE_MCLK_HZ  UINT32_C(32000000)
#define AM13E_APP_SYSTICK_HZ    UINT32_C(16000)
#define AM13E_APP_SYSTICK_CYCLES (AM13E_APP_BASE_MCLK_HZ / AM13E_APP_SYSTICK_HZ)

_Static_assert(AM13E_APP_BASE_MCLK_HZ % AM13E_APP_SYSTICK_HZ == 0,
               "16 kHz SysTick must divide the 32 MHz baseline exactly");
_Static_assert(AM13E_APP_SYSTICK_CYCLES >= 2U &&
               AM13E_APP_SYSTICK_CYCLES <= 0x01000000U,
               "SysTick reload outside Cortex-M33 24-bit range");

/* Application init() is a required board-independent clock contract.
 * Boot verifies the same SYSOSC source/frequency before its handoff.
 * Refuse unexpected MCLK policies rather than silently mis-time Rel17.
 */
void init(void)
{
    if (DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_SYSOSC) {
        for (;;) { __WFI(); }
    }
    if ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) !=
         SYSCTL_CLKSTATUS_SYSOSCFREQ_SYSOSC32M) {
        for (;;) { __WFI(); }
    }
}

/* Called AFTER initgpio/initio and safe motor initialization.
 * SysTick is armed here but PRIMASK is not cleared.
 *
 * TI SDK DL_SYSTICK_init() disables the inherited Boot SysTick
 * before changing LOAD/VAL, then enableInterrupt/enable arm it.
 * CMSIS SetPriority accepts logical priority 0..15, not a
 * pre-shifted raw priority byte. Rel17's PendSV 0x80 value
 * maps to logical midpoint 8 for __NVIC_PRIO_BITS==4.
 */
void am13e_app_motor_runtime_tick_init(void)
{
    init();

    DL_SYSTICK_init(AM13E_APP_SYSTICK_CYCLES);
    NVIC_SetPriority(PendSV_IRQn, (1U << (__NVIC_PRIO_BITS - 1U)));
    NVIC_SetPriority(SysTick_IRQn, 0U);
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();
}
