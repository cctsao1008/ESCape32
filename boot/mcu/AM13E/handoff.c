#include "handoff.h"
#include <soc.h>
#include <stddef.h>
#include <stdint.h>

void boot_am13e_quiesce(void)
{
    boot_am13e_release_io();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    /* APP owns vector dispatch after transfer. Clear external IRQ
     * enable/pending state without changing clocks, priorities, Flash
     * protection or unrelated peripheral registers.
     */
    /* Official AM13E SDK last external IRQ is TMU_LVF_INT (47).
     * Limit writes to implemented IRQ bits, not the CMSIS maximum.
     */
    const unsigned irq_count = (unsigned)TMU_LVF_INT_IRQn + 1U;
    for (size_t i = 0U; i < (irq_count + 31U) / 32U; ++i) {
        const unsigned remaining = irq_count - (unsigned)i * 32U;
        const uint32_t mask = remaining >= 32U ? UINT32_MAX :
                              (UINT32_C(1) << remaining) - 1U;
        NVIC->ICER[i] = mask;
        NVIC->ICPR[i] = mask;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    __DSB();
    __ISB();
}
