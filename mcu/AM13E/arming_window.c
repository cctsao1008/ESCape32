/*
 * ESCape32 Rel17 AM13E: 250ms uninterrupted-neutral arming window.
 *
 * The existing Rel17 main() already owns the arming / throttle policy:
 * it restarts this window whenever its throttle remains nonzero.
 * This backend supplies only the elapsed-time mechanism, derived from
 * the REAL 16kHz SysTick already installed by system_runtime.c.
 *
 * Does not configure a motor, watchdog, timer peripheral or GPIO;
 * specifically does NOT substitute for the still-required real
 * am13e_app_motor_arming_watchdog_refresh() implementation.
 *
 * 32-bit unsigned subtraction works across the SysTick wraparound
 * provided the relevant interval is < 2^31 ticks (~37 hours at 16kHz).
 */
#include "motor_backend.h"
#include "clock_backend.h"
#include <stdint.h>

#define AM13E_APP_ARMING_TICKS (AM13E_APP_SYSTICK_HZ / UINT32_C(4))
_Static_assert(AM13E_APP_SYSTICK_HZ % UINT32_C(4) == 0U,
               "250ms arming window must contain an integer tick count");
_Static_assert(AM13E_APP_ARMING_TICKS == UINT32_C(4000),
               "Rel17 arming timing changed unexpectedly");

/* Defined in original ESCape32 src/main.c and written from
 * SysTick_Handler; its AM13E-only type is volatile to prevent stale
 * reads in the foreground arming loop.
 */
extern volatile uint32_t tick;

static uint32_t window_started_at;
static uint8_t window_active;

void am13e_app_motor_arming_window_start(void)
{
    window_started_at = tick;
    window_active = 1U;
}

int am13e_app_motor_arming_window_expired(void)
{
    if (window_active == 0U) {
        return 0; /* No active window: never silently arm. */
    }
    return (uint32_t)(tick - window_started_at) >=
           AM13E_APP_ARMING_TICKS;
}

void am13e_app_motor_arming_window_restart(void)
{
    if (window_active != 0U) {
        window_started_at = tick;
    }
}

void am13e_app_motor_arming_window_stop(void)
{
    window_active = 0U;
}
