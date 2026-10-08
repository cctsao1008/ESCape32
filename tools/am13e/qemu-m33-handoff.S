.syntax unified
.cpu cortex-m33
.thumb

/* E62 address contract on an MPS2-AN505 Cortex-M33 emulator.
 * This is a CPU handoff harness, NOT the AM13E product Boot ELF.
 */
.section .boot_vectors, "a", %progbits
.align 2
.word 0x28008000
.word boot_reset

.section .boot_code, "ax", %progbits
.align 2
.global boot_reset
.thumb_func
boot_reset:
    ldr r0, =0x00006000
    ldr r1, [r0]
    ldr r2, [r0, #4]
    ldr r3, =0xE000ED08  /* SCB->VTOR */
    str r0, [r3]
    dsb
    isb
    msr msp, r1
    bx r2

.section .app_vectors, "a", %progbits
.align 2
.word 0x28008000
.word app_reset

.section .app_code, "ax", %progbits
.align 2
.global app_reset
.thumb_func
app_reset:
    ldr r0, =0x28000100
    movs r1, #0x5a
    str r1, [r0]
.global app_landed
.thumb_func
app_landed:
    b app_landed
