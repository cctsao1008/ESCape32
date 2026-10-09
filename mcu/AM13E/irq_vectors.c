/*
 * TI AM13E Cortex-M33 exception vector bridge for ESCape32 Rel17.
 * 
 * These are strong CMSIS symbols matching TI startup_gcc_arm.c.
 * Never expose legacy libopencm3 IRQ names as TI vector overrides.
 *
 * The platform must still configure the 16 kHz SysTick, priorities,
 * PendSV ordering and safe HardFault shutdown, and re-enable PRIMASK
 * safely when boot hands control to the application with IRQs disabled.
 */
#include "irq_vectors.h"

void SysTick_Handler(void) {
    sys_tick_handler();
}

void PendSV_Handler(void) {
    pend_sv_handler();
}

void HardFault_Handler(void) {
    hard_fault_handler();
}
