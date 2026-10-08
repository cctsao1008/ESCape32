/*
 * E62 AM13E23019 common bootloader.
 *
 * The current implementation establishes the fixed boot/application image
 * contract and direct application handoff. Service transport and Flash
 * programming are added as part of Boot Porting.
 */

#include <stdbool.h>
#include <stdint.h>
#include "core_cm33.h"
#include "e62_flash_layout.h"

__attribute__((noinline))
static bool e62_boot_app_vector_sane(void)
{
    const volatile uint32_t *vector =
        (const volatile uint32_t *)(uintptr_t)E62_APP_BASE;
    uint32_t initial_sp = vector[0];
    uint32_t reset_pc   = vector[1];
    uint32_t reset_addr = reset_pc & ~1UL;

    if ((initial_sp < E62_RAM_S_BASE) ||
        (initial_sp > E62_RAM_S_END) ||
        ((initial_sp & 0x7UL) != 0UL)) {
        return false;
    }

    if ((reset_pc & 0x1UL) == 0UL) {
        return false;
    }
    if ((reset_addr < E62_APP_BASE) || (reset_addr >= E62_APP_END)) {
        return false;
    }

    return true;
}

__attribute__((noreturn))
static void e62_boot_jump_to_app(void)
{
    const volatile uint32_t *vector =
        (const volatile uint32_t *)(uintptr_t)E62_APP_BASE;
    uint32_t initial_sp = vector[0];
    uint32_t reset_pc   = vector[1];

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    SCB->VTOR = E62_APP_BASE;
    __DSB();
    __ISB();

    __set_CONTROL(0U);
    __ISB();

    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(initial_sp), "r"(reset_pc)
        : "memory");

    __builtin_unreachable();
}

int main(void)
{
    if (e62_boot_app_vector_sane()) {
        e62_boot_jump_to_app();
    }

    /* Invalid/incomplete application: Boot Porting adds programming/recovery. */
    for (;;) {
        __WFI();
    }
}
