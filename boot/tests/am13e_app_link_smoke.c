/*
 * Stage C2 ELF/address-layout smoke only.
 *
 * This target has no motor-control, board pinmux, driver or runtime.
 * Its sole purpose is to prove that the ARM linker can emit the real M33
 * vector table at APP+0; erased signature/header slots follow vectors
 * in the same first 2 KiB sector (at +0x400/+0x500).
 */
#include <stdint.h>

__attribute__((noreturn)) void Reset_Handler(void);

__attribute__((used, section(".intvecs"), aligned(128)))
const uintptr_t am13e_smoke_vector_table[4] = {
    UINT32_C(0x20018000),
    (uintptr_t)&Reset_Handler,
    0U, /* NMI is not installed in the non-executable smoke fixture. */
    0U
};

__attribute__((noreturn)) void Reset_Handler(void) {
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
