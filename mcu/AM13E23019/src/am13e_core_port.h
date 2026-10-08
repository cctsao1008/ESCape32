/* ARM Cortex-M33 CMSIS core operations for rel17 scheduling.
 * Does not configure motor timers, clock tree, or gate-driver outputs.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "cmsis_gcc.h"
#include "core_cm33.h"

/* Replicate rel17's priority byte 0x80 in CMSIS priority encoding. */
static inline void am13e_core_pend_pendsv(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}
static inline void am13e_core_resume_thread(void)
{
    SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;
}
static inline void am13e_core_suspend_thread(void)
{
    SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk;
}
static inline bool am13e_core_start_scheduler_tick(uint32_t core_hz,
                                                      uint32_t tick_hz)
{
    /* No clock tree values are inferred here. SysTick has a 24-bit reload
     * and runs from the Cortex-M33 core clock; reject truncation.
     */
    if (!core_hz || !tick_hz || core_hz % tick_hz) return false;
    const uint32_t ticks = core_hz / tick_hz;
    if (ticks < 2U || ticks > 0x1000000U) return false;
    NVIC_SetPriority(PendSV_IRQn, 0x80U >> (8U - __NVIC_PRIO_BITS));
    SysTick->CTRL = 0U;
    SysTick->LOAD = ticks - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    return true;
}
