/*
 * Explicit AM13E reference board clock provider: 25MHz XTAL -> 200MHz
 * SYSPLL/MCLK. This is NOT AM13E silicon policy or a default BSP.
 * All checks came from the existing reference system_runtime.c path.
 */
#include "board_clock_provider.h"
#include "clock_backend.h"
#include <soc.h>
#include <dl_sysctl.h>

uint32_t am13e_board_clock_start(void)
{
    if (__get_PRIMASK() != 1U ||
        DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_SYSOSC ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) !=
            SYSCTL_CLKSTATUS_SYSOSCFREQ_SYSOSC32M) {
        return 0U;
    }

    const uint32_t hz = am13e_app_clock_configure_xtal25();
    if (hz != AM13E_APP_MCLK_HZ ||
        DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_HSCLK ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_HFCLKGOOD_MASK) !=
            DL_SYSCTL_CLK_STATUS_HFCLK_GOOD ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) !=
            DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_HSCLKGOOD_MASK) !=
            DL_SYSCTL_CLK_STATUS_HSCLK_GOOD) {
        return 0U;
    }
    return hz;
}
