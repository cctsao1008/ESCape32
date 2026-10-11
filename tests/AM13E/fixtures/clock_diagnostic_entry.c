/*
 * ESCape32 AM13E minimal CLOCK/STARTUP executable entry.
 *
 * Explicit intermediate integration target: AM13E_CLOCK.elf.
 * This is NOT the ESCape32 Rel17 FW1 application and MUST NOT
 * be treated as motor-ready, product flashable, or a validated
 * FW1 image. The real Rel17 entry remains src/main.c in AM13E.
 *
 * Uses the actual TI GCC startup, TI DriverLib and AM13E clock
 * backend. Does not create a fake PWM/ADC/BEMF/DSHOT/LED/Watchdog
 * backend merely to make the full FW1 link.
 */
#include <stdint.h>
#include <soc.h>
#include "clock_backend.h"

/* Read with debugger after entering from an explicitly controlled
 * boot environment. 0xC1000001 = init entered; 0xC1000200 =
 * backend reported the nominal agreed 200MHz clock. This is
 * not a crystal-frequency measurement or hardware verification.
 */
volatile uint32_t am13e_clock_diagnostic_state;

/* Existing ESCape32 AM13E system_runtime.c; do not duplicate code. */
extern void init(void);

int main(void)
{
    /* No peripherals and no motor outputs may be intentionally armed. */
    __disable_irq();
    am13e_clock_diagnostic_state = UINT32_C(0xC1000001);

    /* Real 25MHz HFXT -> SYSPLL400 -> MCLK200 backend, fail-closed. */
    init();

    am13e_clock_diagnostic_state = UINT32_C(0xC1000200);

    /* Deliberately keep PRIMASK set; no board-approved IRQ/fault
     * or gate enable sequence exists yet. Debugger can read SRAM.
     */
    for (;;) {
        __NOP();
    }
}
