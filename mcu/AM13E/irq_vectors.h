/*
 * ESCape32 Rel17 <-> TI AM13E Cortex-M33 exception vector contract.
 *
 * TI startup_gcc_arm.c uses uppercase CMSIS interrupt entry names.
 * The original Rel17 application uses lowercase libopencm3 names.
 * These strong wrappers are real dispatches, NOT no-op ISR stubs.
 *
 * Link the wrappers into the Application ELF alongside TI startup.
 * Validate actual .intvecs entries and linker GC in the final ELF.
 */
#pragma once
#if !defined(AM13E)
#error "This exception vector adapter belongs only to AM13E"
#endif

/* Rel17 callbacks implemented in src/main.c */
void sys_tick_handler(void);
void pend_sv_handler(void);
void hard_fault_handler(void);

/* TI SDK startup_gcc_arm.c weak exception names, overridden strongly. */
void SysTick_Handler(void);
void PendSV_Handler(void);
void HardFault_Handler(void);
