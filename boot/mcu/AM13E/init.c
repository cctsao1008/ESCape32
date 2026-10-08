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

    /* The SDK exposes SYSOSC frequency through the CLKSTATUS bitfield.
     * Check the complete field rather than the unavailable USERTRIM token.
     */
    if ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) !=
        SYSCTL_CLKSTATUS_SYSOSCFREQ_SYSOSC32M)
        for (;;) {}
}
