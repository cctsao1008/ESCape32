/*
 * E62 AM13E v2 application LINK SMOKE ONLY.
 * NOT a production motor-controller application linker.
 * The current legacy ESCape32 CMD_WRITE addressing is limited to 256 KiB.
 *
 * Image base  0x00006000 : deferred 0x32EA signature / metadata sector
 * Image hdr   0x00006100 : 32 bytes reserved for v2 packager
 * M33 vectors 0x00006800 : actual initial MSP + Reset_Handler
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH_META (rx) : ORIGIN = 0x00006000, LENGTH = 0x800
    FLASH_APP  (rx) : ORIGIN = 0x00006800, LENGTH = 0x3F800
    RAM_S     (rwx) : ORIGIN = 0x20000000, LENGTH = 0x18000
}

SECTIONS
{
    .signature 0x00006000 : {
        __app_image_start__ = .;
        LONG(0xFFFFFFFF)
    } > FLASH_META

    .image_header 0x00006100 : {
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

    .intvecs 0x00006800 : {
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

    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH_APP

    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH_APP

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

    __StackTop = ORIGIN(RAM_S) + LENGTH(RAM_S);
    /DISCARD/ : { *(.note.GNU-stack) }
}

ASSERT(ADDR(.signature) == 0x00006000, "V2 image must begin at APP 0x6000")
ASSERT(SIZEOF(.signature) == 4, "Only an erased signature word is permitted")
ASSERT(ADDR(.image_header) == 0x00006100, "V2 metadata must be at 0x6100")
ASSERT(SIZEOF(.image_header) == 32, "V2 header must occupy exactly 32 bytes")
ASSERT(ADDR(.intvecs) == 0x00006800, "M33 vectors must begin at 0x6800")
ASSERT(SIZEOF(.intvecs) >= 8, "M33 vectors must contain SP and PC")
/* Strict smoke-only contract: exporting .data is intentionally disabled.
 * Refuse an initialized-data dependency instead of silently omitting it.
 * The real APP linker must support genuine .data load images separately.
 */
ASSERT(SIZEOF(.data) == 0, "Smoke fixture must not contain initialized .data")
ASSERT(__app_flash_payload_end__ <= 0x00046000,
       "E62 v2 legacy CMD_WRITE image exceeds 256 KiB")
