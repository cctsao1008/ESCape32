/*
 * ESCape32 Rel17 AM13E230x reset-cause adapter.
 *
 * Reads the ACTUAL SYSCTL reset cause using the TI AM13E SDK.
 * The original Rel17 policy suppresses startup music following any
 * watchdog reset, and forces the arming window after a WWDG reset.
 *
 * AM13E exposes WWDT0 as BOOTWWDT0. The SDK's Reset Cause register is
 * read-only here; Boot already uses the same DL_SYSCTL_getResetCause().
 *
 * This small backend is BOARD-INDEPENDENT: no assumed GPIO, MCPWM,
 * dead time, or watchdog period. It does not configure a watchdog.
 *
 * IMPORTANT: The separate safety policy for fault reset causes
 * (SYSFLASHECC, BOOTCLKFAIL, etc.) needs a formal platform decision
 * before actual motor operation; this is not a fault classifier.
 */
#include "motor_backend.h"
#include <dl_sysctl.h>

int am13e_app_motor_reset_flags(void)
{
    const DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();

    if (cause == DL_SYSCTL_RESET_CAUSE_BOOTWWDT0) {
        return AM13E_APP_RESET_WATCHDOG | AM13E_APP_RESET_FORCE_ARM;
    }
    return 0;
}
