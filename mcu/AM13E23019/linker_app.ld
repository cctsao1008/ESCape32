/*
 * E62 AM13E23019 application linker script.
 * Derived from TI SDK 26.01.00.03 GCC layout.
 */
_Min_Heap_Size  = 0x1000;
_Min_Stack_Size = 0x1000;

MEMORY
{
    RAM_S       : ORIGIN = 0x20000000, LENGTH = 0x18000
    RAM_C       : ORIGIN = 0x00C18000, LENGTH = 0x8000
    FLASH_IMAGE : ORIGIN = 0x00006000, LENGTH = 0x0007A000
}

REGION_ALIAS("REGION_TEXT", FLASH_IMAGE);
REGION_ALIAS("REGION_PREINIT_ARRAY", FLASH_IMAGE);
REGION_ALIAS("REGION_INIT_ARRAY", FLASH_IMAGE);
REGION_ALIAS("REGION_FINI_ARRAY", FLASH_IMAGE);
REGION_ALIAS("REGION_BSS", RAM_S);
REGION_ALIAS("REGION_NOINIT", RAM_S);
REGION_ALIAS("REGION_DATA", RAM_S);
REGION_ALIAS("REGION_STACK", RAM_S);
REGION_ALIAS("REGION_HEAP", RAM_S);
REGION_ALIAS("REGION_TEXT_RAM", RAM_C);
REGION_ALIAS("REGION_ARM_EXIDX", FLASH_IMAGE);
REGION_ALIAS("REGION_ARM_EXTAB", FLASH_IMAGE);

__e62_app_flash_start__ = ORIGIN(FLASH_IMAGE);
__e62_app_flash_end__ = ORIGIN(FLASH_IMAGE) + LENGTH(FLASH_IMAGE);

SECTIONS
{
    .intvecs ORIGIN(FLASH_IMAGE) : AT (ORIGIN(FLASH_IMAGE)) {
        KEEP (*(.intvecs))
    } > REGION_TEXT

    PROVIDE (_vtable_base_address =
        DEFINED(_vtable_base_address) ? _vtable_base_address : 0x20000000);

    .vtable (_vtable_base_address) (NOLOAD) : AT (_vtable_base_address) {
        KEEP (*(.vtable))
    } > REGION_DATA

    .text : {
        CREATE_OBJECT_SYMBOLS
        KEEP (*(.text))
        . = ALIGN(0x8);
        *(.text.*)
        . = ALIGN(0x8);
        KEEP (*(.ctors))
        . = ALIGN(0x8);
        KEEP (*(.dtors))
        . = ALIGN(0x8);
        KEEP (*(.init))
        . = ALIGN(0x8);
        KEEP (*(.fini*))
        . = ALIGN(0x8);
    } > REGION_TEXT AT> REGION_TEXT

    .TI.ramfunc : {
        __ramfunct_load__ = LOADADDR(.TI.ramfunc);
        __ramfunct_start__ = .;
        *(.TI.ramfunc)
        . = ALIGN(0x8);
        __ramfunct_end__ = .;
    } > REGION_TEXT_RAM AT> REGION_TEXT

    .rodata : {
        *(.rodata)
        . = ALIGN(0x8);
        *(.rodata.*)
        . = ALIGN(0x8);
    } > REGION_TEXT AT> REGION_TEXT

    .preinit_array : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*));
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > REGION_PREINIT_ARRAY AT> REGION_TEXT

    .init_array : {
        . = ALIGN(0x8);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > REGION_INIT_ARRAY AT> REGION_TEXT

    .fini_array : {
        . = ALIGN(0x8);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(0x8);
    } > REGION_FINI_ARRAY AT> REGION_TEXT

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > REGION_ARM_EXIDX AT> REGION_ARM_EXIDX

    .ARM.extab : {
        . = ALIGN(0x8);
        KEEP (*(.ARM.extab* .gnu.linkonce.armextab.*))
        . = ALIGN(0x8);
    } > REGION_ARM_EXTAB AT> REGION_ARM_EXTAB

    __etext = .;

    .data : {
        __data_load__ = LOADADDR(.data);
        __data_start__ = .;
        KEEP (*(.data))
        KEEP (*(.data*))
        . = ALIGN(8);
        __data_end__ = .;
    } > REGION_DATA AT> REGION_TEXT

    .bss : {
        __bss_start__ = .;
        *(.shbss)
        KEEP (*(.bss))
        *(.bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > REGION_BSS AT> REGION_BSS

    .noinit : {
        KEEP(*(*.noinit*))
        . = ALIGN(8);
    } > REGION_NOINIT AT> REGION_NOINIT

    .heap : {
        __heap_start__ = .;
        end = __heap_start__;
        _end = end;
        __end = end;
        KEEP (*(.heap))
        __heap_end__ = .;
        __HeapLimit = __heap_end__;
    } > REGION_HEAP AT> REGION_HEAP

    .stack (NOLOAD) : ALIGN(0x8) {
        _stack = .;
        KEEP(*(.stack))
    } > REGION_STACK AT> REGION_STACK

    __StackTop = ORIGIN(REGION_STACK) + LENGTH(REGION_STACK);
    PROVIDE(__stack = __StackTop);
}

ASSERT(ADDR(.intvecs) == 0x00006000, "E62 APP vector table must start at 0x00006000");
ASSERT(__e62_app_flash_start__ == 0x00006000, "E62 APP start mismatch");
ASSERT(__e62_app_flash_end__ == 0x00080000, "E62 APP end mismatch");
