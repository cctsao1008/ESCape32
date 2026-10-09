/*
 * AM13E / ESCape32 Rel17 Application LINKER REFERENCE — NOT FLASHABLE.
 *
 * First establish linker and TI Startup section contracts, then implement
 * real board peripherals/fault handling and validate on hardware.
 * Do NOT wire this file into a production image without qualification.
 *
 * This is based on the currently agreed Boot reference layout. The
 * legacy CMD_WRITE packed-image transport currently caps the firmware
 * image to 256 KiB (0x6000..0x45FFF), NOT all physical main Flash.
 *
 * Flash map (addresses are reference, not independently qualified):
 * 0x00000000..0x00003FFF  Boot (16 KiB)
 * 0x00004000..0x00005FFF  Parameter storage (8 KiB, separate from App)
 * 0x00006000..0x000067FF  Image signature/metadata sector (2 KiB)
 * 0x00006800..0x00045FFF  Application vectors, code, data load image
 *
 * TI startup_gcc_arm.c owns .data/.TI.ramfunc copy and .bss clear.
 * Rel17 main() explicitly copies persistent _cfg Flash into .cfg SRAM.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH_BOOT (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00004000
    FLASH_CFG  (rx)  : ORIGIN = 0x00004000, LENGTH = 0x00002000
    FLASH_META (rx)  : ORIGIN = 0x00006000, LENGTH = 0x00000800
    FLASH_APP  (rx)  : ORIGIN = 0x00006800, LENGTH = 0x0003F800
    RAM_S      (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00018000
    RAM_C      (rwx) : ORIGIN = 0x00C18000, LENGTH = 0x00008000
}

__boot_flash_start__    = ORIGIN(FLASH_BOOT);
__boot_flash_end__      = ORIGIN(FLASH_BOOT) + LENGTH(FLASH_BOOT);
__boot_storage_end__    = 0x00080000;
__app_flash_start__     = ORIGIN(FLASH_META);
__app_vector_start__    = ORIGIN(FLASH_APP);

_boot = ORIGIN(FLASH_BOOT);
_cfg  = ORIGIN(FLASH_CFG);  /* Persistent Flash *source*, not .cfg SRAM. */
_rom  = ORIGIN(FLASH_APP);
_ram  = ORIGIN(RAM_S);
__StackTop = ORIGIN(RAM_S) + LENGTH(RAM_S);

SECTIONS
{
    .signature ORIGIN(FLASH_META) : {
        __app_image_start__ = .;
        LONG(0xFFFFFFFF)  /* Signature-last boot commit handled by packer. */
    } > FLASH_META

    .image_header ORIGIN(FLASH_META) + 0x100 : {
        __app_image_header_start__ = .;
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        __app_image_header_end__ = .;
    } > FLASH_META

    .intvecs ORIGIN(FLASH_APP) : {
        __app_vectors_begin__ = .;
        KEEP(*(.intvecs))
        __app_vectors_end__ = .;
    } > FLASH_APP

    .text : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.glue_7 .glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        KEEP(*(.preinit_array .preinit_array.*))
        KEEP(*(.init_array .init_array.*))
        KEEP(*(.fini_array .fini_array.*))
        . = ALIGN(8);
    } > FLASH_APP

    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH_APP
    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
    } > FLASH_APP

    /*
     * Only explicit SRAM-resident routines belong here. TI Startup copies
     * this section before main(). Any Flash DriverLib call graph used to
     * erase/program executing Flash must be verified separately in MAP.
     */
    .TI.ramfunc : {
        __ramfunct_start__ = .;
        *(.TI.ramfunc .TI.ramfunc.*)
        . = ALIGN(4);
        __ramfunct_end__ = .;
    } > RAM_C AT> FLASH_APP
    __ramfunct_load__ = LOADADDR(.TI.ramfunc);

    .data : {
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM_S AT> FLASH_APP
    __data_load__ = LOADADDR(.data);

    /*
     * Mutable config is NOT startup .data. Rel17 main() copies persistent
     * config at _cfg (Flash) into this SRAM working section, then validates
     * it using cfgdata as factory defaults when required.
     */
    .cfg (NOLOAD) : {
        . = ALIGN(8);
        _cfg_start = .;
        KEEP(*(.cfg))
        . = ALIGN(8);
        _cfg_end = .;
    } > RAM_S

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM_S

    .noinit (NOLOAD) : {
        *(.noinit .noinit.*)
        . = ALIGN(8);
    } > RAM_S

    __app_flash_payload_end__ = LOADADDR(.data) + SIZEOF(.data);
    _eod = __app_flash_payload_end__;

    /DISCARD/ : { *(.note.GNU-stack) }
}

ASSERT(ADDR(.signature) == 0x00006000, "App signature address mismatch")
ASSERT(SIZEOF(.signature) == 4, "App erased-signature sentinel incorrect")
ASSERT(ADDR(.image_header) == 0x00006100, "App header address mismatch")
ASSERT(SIZEOF(.image_header) == 32, "App metadata must be 32 bytes")
ASSERT(ADDR(.intvecs) == 0x00006800, "App M33 vectors must begin 0x6800")
ASSERT(SIZEOF(.intvecs) >= 64, "App must contain a real M33 vector table")
ASSERT(_cfg_end - _cfg_start <= LENGTH(FLASH_CFG),
       "Serialized config exceeds reserved parameter storage")
ASSERT(__app_flash_payload_end__ <= 0x00046000,
       "Packed application exceeds 256 KiB CMD_WRITE transport")
ASSERT(__data_start__ >= ORIGIN(RAM_S) &&
       __bss_end__ <= ORIGIN(RAM_S) + LENGTH(RAM_S),
       "Application RAM_S sections overflow")
ASSERT(__ramfunct_end__ <= ORIGIN(RAM_C) + LENGTH(RAM_C),
       "Application RAM_C section overflow")
