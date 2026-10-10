/* ESCape32 Rel17 valid-input hardware watchdog on AM13E23019.
 *
 * WWDT0 is not an MCU-output-disable substitute: hardware CMPSS ->
 * PWMXBAR -> MCPWM Trip Zone remains independently mandatory.
 *
 * TI SDK 26.01.00.03 dl_wwdt.h:
 *   WWDT0 clock = LFCLK. Period 2^15 / 32768Hz = nominal 1 second.
 *   0% closed window permits repeated DShot600 valid-command feeding.
 *   Watchdog starts on FIRST VALID INPUT, not early during startup,
 *   and is NEVER restarted from SysTick/housekeeping.
 *
 * Precise LFCLK frequency, Boot watchdog ownership, reset behavior and
 * physical loss-of-command latency must be verified on AM13E silicon.
 * These selected WWDT settings are a software policy to qualify against
 * Rel17 input-loss behavior, not an approved board-level timeout.
 */
#include "io_backend.h"
#include "input_watchdog.h"
#include <soc.h>
#include <dl_wwdt.h>
#include <dl_sysctl.h>
#include <stdint.h>

_Static_assert(DL_WWDT_WINDOW_PERIOD_0 == WWDT_WWDTCTL0_WINDOW0_SIZE_0,
               "WWDT0 no-closed-window encoding changed");

static volatile uint32_t prepared;
static volatile uint32_t armed;
static volatile uint32_t valid_frame_feeds;

static void watchdog_fail_closed(void)
{
    __disable_irq();
    hard_fault_handler();
    for (;;) { __NOP(); }
}

void am13e_app_io_watchdog_prepare(void)
{
    /* Called by initio() during Boot PRIMASK=1 handoff.
     * Do not reprogram or inherit an unknown already-running Boot WWDT.
     */
    if (prepared || __get_PRIMASK() == 0U) watchdog_fail_closed();
    DL_WWDT_enablePower(WWDT0);
    if (!DL_WWDT_isPowerEnabled(WWDT0) ||
        DL_WWDT_isRunning(WWDT0)) watchdog_fail_closed();

    /* Original Rel17 reset cause logic maps BOOTWWDT0 to re-arming.
     * An NMI-only WWDT policy would violate that existing contract.
     */
    DL_SYSCTL_setWWDT0ErrorBehavior(DL_SYSCTL_ERROR_BEHAVIOR_RESET);
    if (DL_SYSCTL_getWWDT0ErrorBehavior() !=
        DL_SYSCTL_ERROR_BEHAVIOR_RESET) watchdog_fail_closed();

    prepared = 1U; /* WWDT is powered, intentionally NOT started. */
}

void am13e_app_io_watchdog_feed(void)
{
    /* This function is called only after original Rel17 verifies DShot
     * CRC or the decoder verifies the receiver PWM width and period.
     * In particular a repeated CRC-bad command CANNOT prevent reset.
     */
    if (!prepared || !DL_WWDT_isPowerEnabled(WWDT0))
        watchdog_fail_closed();

    if (!armed) {
        /* The TI driver starts the real WWDT automatically here. The
         * first accepted command establishes timeout supervision.
         * Nominal one-second hardware expiry if valid frames stop.
         */
        DL_WWDT_initWatchdogMode(WWDT0,
                                DL_WWDT_CLOCK_DIVIDE_1,
                                DL_WWDT_TIMER_PERIOD_15_BITS,
                                DL_WWDT_RUN_IN_SLEEP,
                                DL_WWDT_WINDOW_PERIOD_0,
                                DL_WWDT_WINDOW_PERIOD_0);
        armed = 1U;
    } else {
        /* Full-open window: legal to restart on every accepted packet. */
        if (!DL_WWDT_isRunning(WWDT0)) watchdog_fail_closed();
        DL_WWDT_restart(WWDT0);
    }
    ++valid_frame_feeds;
}

void am13e_app_io_watchdog_status(uint32_t *is_armed, uint32_t *feeds)
{
    if (is_armed != (void *)0) *is_armed = armed;
    if (feeds != (void *)0) *feeds = valid_frame_feeds;
}
