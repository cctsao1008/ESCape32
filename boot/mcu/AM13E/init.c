/*
** AM13E boot clock initialization.
** Preserve the reset-default internal 32 MHz SYSOSC clock.
** No board oscillator, PLL, or motor-control clocks are configured.
*/
#include "common.h"
#include <dl_sysctl.h>

void init(void) {
    /* SYSOSC is the hardware reset MCLK source on AM13E.
     * Refuse to proceed if a non-reset clock configuration is inherited.
     * The boot service clock contract requires 32 MHz SYSOSC.
     */
    if (DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_SYSOSC)
        for (;;) {}

    if (DL_SYSCTL_getClockStatus() &
        (DL_SYSCTL_CLK_STATUS_SYSOSC_4MHZ |
         DL_SYSCTL_CLK_STATUS_SYSOSC_USERTRIM_FREQ))
        for (;;) {}
}
