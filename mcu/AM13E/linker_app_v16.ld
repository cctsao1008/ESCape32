/*
 * AM13E reference AM13E23019 FW1 Application Linker — SW Architecture v1.6.
 * DETAILS: APP_BASE=0x6000 vectors, marker=APP+0x400, CRC header=APP+0x500.
 *
 * This is a fully linked FW1 firmware candidate, NOT hardware-qualified.
 * Fixed 16KiB Boot, separate FW1/FW2 4KiB parameter regions, physical
 * 488KiB shared APP allocation. AM13E Boot CMD_WINDOW=6 covers
 * all 488 logical 1KiB blocks; older WiFi-Link clients need an explicit
 * window-aware update to transfer >256KiB images.
 * Input/output markers, CRC and signature-last commit are validated by
 * Boot/Packer host tests. Do NOT program power hardware on CI evidence.
 *
 * CODE/RODATA starts at 0x6800, outside the first 2KiB sector containing
 * the vector table and the two reserved metadata windows. The boot image
 * is application-agnostic; FW1 and FW2 use the same Image Contract.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH_BOOT (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00004000
    FLASH_CFG1 (rx) : ORIGIN = 0x00004000, LENGTH = 0x00001000
    FLASH_CFG2 (rx) : ORIGIN = 0x00005000, LENGTH = 0x00001000
    FLASH_META (rx) : ORIGIN = 0x00006000, LENGTH = 0x00000800
    FLASH_APP  (rx) : ORIGIN = 0x00006800, LENGTH = 0x00079800
    RAM_S      (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00018000
    RAM_C      (rwx) : ORIGIN = 0x00C18000, LENGTH = 0x00008000
}

__boot_flash_start__    = ORIGIN(FLASH_BOOT);
__boot_flash_end__      = ORIGIN(FLASH_BOOT) + LENGTH(FLASH_BOOT);
__boot_storage_end__    = 0x00080000;
__app_flash_start__     = ORIGIN(FLASH_META);
__app_vector_start__    = ORIGIN(FLASH_META);

_boot = ORIGIN(FLASH_BOOT);
_cfg  = ORIGIN(FLASH_CFG1);  /* FW1 source ONLY; never FW2 config. */
_rom  = ORIGIN(FLASH_APP);
_ram  = ORIGIN(RAM_S);
__StackTop = ORIGIN(RAM_S) + LENGTH(RAM_S);

SECTIONS
{
    .intvecs ORIGIN(FLASH_META) : {
        __app_image_start__ = .;
        __app_vectors_begin__ = .;
        KEEP(*(.intvecs))
        __app_vectors_end__ = .;
    } > FLASH_META

    .signature ORIGIN(FLASH_META) + 0x400 : {
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
        LONG(0xFFFFFFFF)
    } > FLASH_META

    .image_header ORIGIN(FLASH_META) + 0x500 : {
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

ASSERT(ADDR(.intvecs) == 0x00006000, "v1.6 vectors must be at APP_BASE")
ASSERT(SIZEOF(.intvecs) >= 64, "Real M33 vector table required")
ASSERT(SIZEOF(.intvecs) <= 0x400, "Vector table must not reach deferred marker")
ASSERT(ADDR(.signature) == 0x00006400, "Signature must be in APP+0x400 ECC16")
ASSERT(SIZEOF(.signature) == 16, "Marker slot must use exactly one ECC16")
ASSERT(ADDR(.image_header) == 0x00006500, "Header must be at APP+0x500")
ASSERT(SIZEOF(.image_header) == 32, "Header is exactly 32 bytes")
ASSERT(SIZEOF(.intvecs) >= 64, "App must contain a real M33 vector table")
ASSERT(_cfg == 0x00004000, "FW1 parameter source must be 0x4000")
ASSERT(_cfg_end - _cfg_start <= LENGTH(FLASH_CFG1),
       "FW1 serialized config exceeds its OWN 4KiB region")
ASSERT(ORIGIN(FLASH_CFG2) == 0x5000 && LENGTH(FLASH_CFG2)==0x1000,
       "Independent FW2 config region must be preserved")
ASSERT(ORIGIN(FLASH_APP) + LENGTH(FLASH_APP) == 0x00080000,
       "Physical application allocation must end at 512KiB Flash")
ASSERT(__app_flash_payload_end__ <= 0x00080000,
       "Packed application exceeds 488 KiB partition (CMD_WINDOW transport)")
ASSERT(__data_start__ >= ORIGIN(RAM_S) &&
       __bss_end__ <= ORIGIN(RAM_S) + LENGTH(RAM_S),
       "Application RAM_S sections overflow")
ASSERT(__ramfunct_end__ <= ORIGIN(RAM_C) + LENGTH(RAM_C),
       "Application RAM_C section overflow")
