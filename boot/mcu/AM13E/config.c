/*
** AM13E native Boot config.c: original init() and verified SYSOSC.
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

#ifndef AM13E_CLOCK_DIAGNOSTIC
/* ESCape32 Rel17 / Rev1.6 AM13E Boot entry bridge.
 * The single TI SDK Reset_Handler declares extern int main(void);
 * Boot's original protocol dispatcher retains a void-return contract
 * in boot/src/main.c and its legacy MCU entry is not modified.
 * No additional vector table, framework, Boot ABI or parser is added.
 */
void am13e_rel17_boot_main(void);

int main(void)
{
    am13e_rel17_boot_main();
    /* Unexpected return from the original Boot command loop is not a
     * successful APP handoff. Remain in Boot rather than returning to
     * TI Startup or introducing a second reset/launch path.
     */
    for (;;) {
    }
}

#endif
