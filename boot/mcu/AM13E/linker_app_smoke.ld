/*
 * ESCape32 Rel17 v1.4 Cortex-M33 flat-binary link smoke.
 * 488KiB is the maximum physical region, not the output file length.
 * No APP signature or CRC/header reservations.
 */
ENTRY(Reset_Handler)
MEMORY
{
    FLASH_APP (rx) : ORIGIN = 0x00006000, LENGTH = 0x0007A000
    RAM_S (rwx) : ORIGIN = 0x20000000, LENGTH = 0x18000
}
SECTIONS
{
    .intvecs 0x00006000 : {
        __app_image_start__ = .;
        __app_vector_start__ = .;
        KEEP(*(.intvecs))
        __app_vector_end__ = .;
    } > FLASH_APP
    .text : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.glue_7 .glue_7t)
        . = ALIGN(8);
    } > FLASH_APP
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH_APP
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH_APP
    __app_flash_payload_end__ = .;
    .data : {
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(8);
        __data_end__ = .;
    } > RAM_S AT> FLASH_APP
    __data_load__ = LOADADDR(.data);
    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > RAM_S
    __StackTop = ORIGIN(RAM_S)+LENGTH(RAM_S);
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(ADDR(.intvecs)==0x6000,"M33 vectors at APP_BASE")
ASSERT(SIZEOF(.intvecs)>=8,"M33 initial SP + PC required")
ASSERT(SIZEOF(.data)==0,"Smoke must not need initialized .data")
ASSERT(__app_flash_payload_end__<=0x80000,"Maximum APP = 488KiB")
