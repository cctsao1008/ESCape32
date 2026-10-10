/* Rel17 v1.4 AM13E native Boot SRAM reception, not a Flash commit. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../../mcu/AM13E/flash_partition.h"
#define AM13E_BOOT_STAGE_BYTES (AM13E_FLASH_BOOT_END-AM13E_FLASH_BOOT_BASE)
#define AM13E_BOOT_STAGE_BLOCK_BYTES AM13E_FLASH_LOGICAL_BLOCK
#define AM13E_BOOT_STAGE_BLOCKS (AM13E_BOOT_STAGE_BYTES/AM13E_BOOT_STAGE_BLOCK_BYTES)
void boot_am13e_stage_begin(void);
void boot_am13e_stage_abort(void);
uint8_t *boot_am13e_stage_next(unsigned index);
bool boot_am13e_stage_accept(unsigned index,unsigned length);
unsigned boot_am13e_stage_length(void);
bool boot_am13e_stage_complete(void);
bool boot_am13e_stage_vector_plausible(void);
