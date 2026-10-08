ENTRY(boot_reset)

MEMORY {
  EMU_FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00010000
  EMU_RAM (rwx) : ORIGIN = 0x28000000, LENGTH = 0x00010000
}
SECTIONS {
  .boot_vectors 0x00000000 : { KEEP(*(.boot_vectors)) } > EMU_FLASH
  .boot_code : { *(.boot_code) *(.boot_code.*) } > EMU_FLASH
  .app_vectors 0x00006000 : { KEEP(*(.app_vectors)) } > EMU_FLASH
  .app_code : { *(.app_code) *(.app_code.*) } > EMU_FLASH
  /DISCARD/ : { *(.note*) *(.comment*) *(.ARM.attributes) }
}
ASSERT(ADDR(.boot_vectors) == 0, "Boot vectors wrong");
ASSERT(ADDR(.app_vectors) == 0x6000, "APP vectors wrong");
ASSERT(SIZEOF(.boot_code) < 0x6000, "Boot code overlaps APP vectors");
