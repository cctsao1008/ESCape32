/*
 * Stage C2 ELF/address-layout smoke only.
 *
 * This target has no motor-control, board pinmux, driver or runtime.
 * Its sole purpose is to prove that the ARM linker can emit the real M33
 * vector table at APP+0x800 behind an erased signature/header sector.
 */
#include <stdint.h>

__attribute__((noreturn)) void Reset_Handler(void);

__attribute__((used, section(".intvecs"), aligned(128)))
const uintptr_t e62_smoke_vector_table[2] = {
    UINT32_C(0x20018000),
    (uintptr_t)&Reset_Handler
};

__attribute__((noreturn)) void Reset_Handler(void) {
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
