/*
 * Native-host timing regression for the ACTUAL AM13E arming backend.
 * No peripheral or watchdog emulation; this checks only arithmetic
 * and API state transitions. For target compilation use ARM GCC.
 */
#include "../motor_backend.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

volatile uint32_t tick;

int main(void)
{
    /* No explicit start must never signal "safe to arm". */
    tick = 100U;
    assert(!am13e_app_motor_arming_window_expired());

    /* Exactly 4000 ticks = 250ms at 16kHz. */
    am13e_app_motor_arming_window_start();
    tick = 4099U;
    assert(!am13e_app_motor_arming_window_expired());
    tick = 4100U;
    assert(am13e_app_motor_arming_window_expired());

    /* Non-neutral input restarts the continuous-neutral window. */
    tick = 9000U;
    am13e_app_motor_arming_window_start();
    tick = 11999U;
    assert(!am13e_app_motor_arming_window_expired());
    am13e_app_motor_arming_window_restart();
    tick = 15998U;
    assert(!am13e_app_motor_arming_window_expired());
    tick = 15999U;
    assert(am13e_app_motor_arming_window_expired());

    /* 32-bit tick wraparound must remain correct. */
    tick = UINT32_MAX - 100U;
    am13e_app_motor_arming_window_start();
    tick = 3898U; /* UINT32_MAX-100 + 3999 modulo 2^32 */
    assert(!am13e_app_motor_arming_window_expired());
    tick = 3899U;
    assert(am13e_app_motor_arming_window_expired());

    /* Stop must invalidate old elapsed times; restart inactive is inert. */
    am13e_app_motor_arming_window_stop();
    tick = 90000U;
    am13e_app_motor_arming_window_restart();
    assert(!am13e_app_motor_arming_window_expired());

    puts("PASS: ESCape32 AM13E 250ms arming window boundary/restart/wrap");
    return 0;
}
