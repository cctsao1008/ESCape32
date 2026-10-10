/* ARM linker-only smoke: native M33 vectors then ordinary linked text.
 * No metadata/CRC marker is reserved or written.
 */
#include <stdint.h>
__attribute__((noreturn)) void Reset_Handler(void);
__attribute__((used,section(".intvecs"),aligned(128)))
const uintptr_t am13e_smoke_vector_table[4] = {
    UINT32_C(0x20018000), (uintptr_t)&Reset_Handler, 0U, 0U
};
__attribute__((noreturn)) void Reset_Handler(void) {
    for(;;) { __asm__ volatile("wfi"); }
}
