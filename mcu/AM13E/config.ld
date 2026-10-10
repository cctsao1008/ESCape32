/*
 * ESCape32 Rel17 v1.4 native AM13E23019 Application config.ld.
 * FLASH APP is 0x6000..0x7ffff (488KiB MAXIMUM, NOT fixed image size).
 * Cortex-M33 vectors begin at APP_BASE. Code follows the actual linked
 * vector table; no v1.6 APP+0x400 signature or APP+0x500 CRC header.
 * Cfg.id = 0x32EA lives independently at 0x4000.
 */
ENTRY(Reset_Handler)
MEMORY
{
    FLASH_BOOT     (rx) : ORIGIN = 0x00000000, LENGTH = 0x00004000
    FLASH_CFG1     (rx) : ORIGIN = 0x00004000, LENGTH = 0x00001000
    FLASH_RESERVED (rx) : ORIGIN = 0x00005000, LENGTH = 0x00001000
    FLASH_APP      (rx) : ORIGIN = 0x00006000, LENGTH = 0x0007A000
    RAM_S         (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00018000
    RAM_C         (rwx) : ORIGIN = 0x00C18000, LENGTH = 0x00008000
}
__boot_flash_start__ = ORIGIN(FLASH_BOOT);
__boot_flash_end__ = ORIGIN(FLASH_BOOT)+LENGTH(FLASH_BOOT);
__cfg_flash_start__ = ORIGIN(FLASH_CFG1);
__boot_storage_end__ = 0x00080000;
__app_flash_start__ = ORIGIN(FLASH_APP);
__app_vector_start__ = ORIGIN(FLASH_APP);
_boot = ORIGIN(FLASH_BOOT);
_cfg = ORIGIN(FLASH_CFG1);
_rom = ORIGIN(FLASH_APP);
_ram = ORIGIN(RAM_S);
__StackTop = ORIGIN(RAM_S)+LENGTH(RAM_S);
SECTIONS
{
    .intvecs ORIGIN(FLASH_APP) : {
        __app_image_start__ = .;
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
    __app_flash_payload_end__ = LOADADDR(.data)+SIZEOF(.data);
    _eod = __app_flash_payload_end__;
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(ADDR(.intvecs)==0x6000, "M33 vectors must be at APP_BASE = 0x6000")
ASSERT(SIZEOF(.intvecs)>=64, "Full M33 vector table required")
ASSERT(_cfg==0x4000, "Upstream ESCape32 Cfg.id source must be at 0x4000")
ASSERT(_cfg_end-_cfg_start<=LENGTH(FLASH_CFG1),"ESCape32 Cfg exceeds 4KiB")
ASSERT(ORIGIN(FLASH_RESERVED)==0x5000 &&
       LENGTH(FLASH_RESERVED)==0x1000,"Reserved 4KiB must be preserved")
ASSERT(ORIGIN(FLASH_APP)+LENGTH(FLASH_APP)==0x80000,
       "APP partition must end at 512KiB MAIN Flash")
ASSERT(__app_flash_payload_end__<=0x80000,
       "Actual linked firmware exceeds 488KiB APP partition")
ASSERT(__data_start__>=ORIGIN(RAM_S) &&
       __bss_end__<=ORIGIN(RAM_S)+LENGTH(RAM_S),"RAM_S overflow")
ASSERT(__ramfunct_end__<=ORIGIN(RAM_C)+LENGTH(RAM_C),
       "RAM_C overflow")
