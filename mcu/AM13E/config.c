/*
 * AM13E native Application config.c: original init() and 16kHz SysTick.
 *
 * The explicitly selected board clock provider owns ALL oscillator,
 * SYSPLL, MCLK, and Flash wait-state configuration and readback.
 * This source owns only the application init handshake and 16kHz
 * original-Rel17 SysTick/IRQ policy; it does not select a crystal.
 */
#include "common.h"
#include "motor_backend.h"
#include "runtime_tick_contract.h"
#include "board_clock_provider.h"
#include <dl_systick.h>

static uint32_t app_mclk_hz;

void init(void)
{
    if (__get_PRIMASK() != 1U || app_mclk_hz != 0U) {
        for (;;) { __NOP(); }
    }
    /* No weak fallback: without a board provider this firmware cannot
     * link. The provider must verify actual clock source and divider.
     */
    const uint32_t hz = am13e_board_clock_start();
    const uint32_t cycles = hz / AM13E_APP_SYSTICK_HZ;
    if (hz == 0U ||
        hz % AM13E_APP_SYSTICK_HZ != 0U ||
        cycles < 2U ||
        cycles > AM13E_APP_SYSTICK_MAX_CYCLES ||
        __get_PRIMASK() != 1U) {
        for (;;) { __NOP(); }
    }
    app_mclk_hz = hz;
}

void am13e_app_motor_runtime_tick_init(void)
{
    if (app_mclk_hz == 0U || __get_PRIMASK() != 1U) {
        for (;;) { __NOP(); }
    }
    DL_SYSTICK_init(app_mclk_hz / AM13E_APP_SYSTICK_HZ);
    NVIC_SetPriority(PendSV_IRQn, (1U << (__NVIC_PRIO_BITS - 1U)));
    NVIC_SetPriority(SysTick_IRQn, 0U);
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();
}

/* ESCape32 Rel17 / Rev1.6 AM13E Application entry bridge.
 * TI SDK startup_gcc_arm.c declares extern int main(void). Keep one
 * startup/exception vector provider in the TI startup object and
 * preserve Rel17's original void control-loop body in src/main.c.
 * The entry bridge adds no clock, peripheral or control policy.
 */
void am13e_rel17_app_main(void);

int main(void)
{
    am13e_rel17_app_main();
    /* Rel17's main loop should never return. Do not restart firmware
     * or jump to a different image on an unexpected return.
     */
    for (;;) {
    }
}
