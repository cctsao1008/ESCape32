/*
 * AM13E23019 boot platform initialization.
 */

#include "boot_platform.h"

#include "dl_sysctl.h"
#include "flash_layout.h"
#include "soc.h"

bool boot_platform_init(void)
{
    /*
     * A SYSRST returns MCLK to SYSOSC. Keep the transition explicit so Boot
     * does not inherit an application HSCLK/PLL assumption if entry behavior
     * changes later. The Boot timing contract uses the normal 32-MHz SYSOSC
     * base mode and explicitly rejects the 4-MHz SYSOSC mode below.
     */
    if (DL_SYSCTL_getMCLKSource() == DL_SYSCTL_MCLK_SOURCE_HSCLK) {
        DL_SYSCTL_switchMCLKfromHSCLKtoSYSOSC();
    }

    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIV_1_DIV_1);

    uint32_t clock_status = DL_SYSCTL_getClockStatus();
    if ((clock_status & DL_SYSCTL_CLK_STATUS_SYSOSC_4MHZ) != 0U) {
        return false;
    }

    /* Boot owns the vector table until the explicit APP handoff. */
    SCB->VTOR = ESCAPE32_BOOT_BASE;

    /* PB14 transport uses SysTick as a polling timebase. Start from disabled. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    __DSB();
    __ISB();

    return true;
}
