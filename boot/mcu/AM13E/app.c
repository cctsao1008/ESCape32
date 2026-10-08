/*
** AM13E application validation and reset handoff.
** Reference image contract: ESCape32 signature at image base and
** Cortex-M33 vector table one 2 KiB Flash sector after image base.
** Application linker layout must match before hardware use.
*/
#include "common.h"
#include <stdint.h>
#include <soc.h>
#include "image_integrity.h"

extern char __app_flash_start__[];
extern char __app_vector_start__[];
extern char __boot_storage_end__[];

static bool boot_app_vectors(uint32_t *sp, uint32_t *pc) {
    const uintptr_t image = (uintptr_t)__app_flash_start__;
    const uintptr_t vector = (uintptr_t)__app_vector_start__;
    const uintptr_t end = (uintptr_t)__boot_storage_end__;
    if (image >= vector || vector > end || end - vector < 8U ||
        (vector & 127U) != 0U)
        return false;

    if (*(const volatile uint16_t *)image != UINT16_C(0x32ea))
        return false;

    const volatile uint32_t *table = (const volatile uint32_t *)vector;
    const uint32_t initial_sp = table[0];
    const uint32_t reset_pc = table[1];

    /* AM13E reference linker: secure SRAM 0x20000000..0x20017FFF. */
    if ((initial_sp & 7U) != 0U ||
        initial_sp < UINT32_C(0x20000008) ||
        initial_sp > UINT32_C(0x20018000))
        return false;
    if ((reset_pc & 1U) == 0U ||
        (uintptr_t)(reset_pc & ~UINT32_C(1)) < vector ||
        (uintptr_t)(reset_pc & ~UINT32_C(1)) >= end)
        return false;

    *sp = initial_sp;
    *pc = reset_pc;
    return true;
}

bool boot_am13e_application_valid(void) {
    const uintptr_t first = (uintptr_t)__app_flash_start__;
    const uintptr_t end = (uintptr_t)__boot_storage_end__;
    /* Check the *committed* Flash image on every cold boot, not just
     * the ESCape32 signature and plausible M33 entry address.
     */
    if ((uintptr_t)__app_vector_start__ !=
            first + AM13E_IMAGE_VECTOR_OFFSET ||
        boot_am13e_image_check(first, end, 0, 0U, 0) !=
            AM13E_IMAGE_VALID)
        return false;
    uint32_t sp, pc;
    return boot_app_vectors(&sp, &pc);
}

__attribute__((noreturn)) void boot_am13e_launch_application(void) {
    uint32_t sp, pc;
    if (!boot_am13e_application_valid() ||
        !boot_app_vectors(&sp, &pc))
        for (;;) {}

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->VTOR = (uint32_t)(uintptr_t)__app_vector_start__;
    __DSB();
    __ISB();

    __set_MSP(sp);
    __asm__ volatile ("bx %0" :: "r"(pc) : "memory");
    __builtin_unreachable();
}
