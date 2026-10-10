/* ESCape32 Rel17 v1.4 application launch gate.
 * The original 0x32EA marker belongs to the persistent Cfg region at
 * 0x4000, NOT to application code or an AM13E image header.
 * The 488 KiB APP region is a CAPACITY, never a fixed image length.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../../mcu/AM13E/flash_partition.h"
#define AM13E_BOOT_CFG_ID UINT16_C(0x32ea)
bool boot_am13e_app_validity(const void *cfg_flash,
                             const void *app_vectors,
                             uint32_t *initial_sp,
                             uint32_t *reset_pc);
