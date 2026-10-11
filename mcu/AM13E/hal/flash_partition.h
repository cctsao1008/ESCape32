/* Shared Boot/FW1 AM13E23019 Rel17 / Rev1.6 single-image Flash allocation.
 * Source constants, NOT a new image format or FW2 feature API.
 * Both Boot and FW1 include this file. Linker LD assertions independently
 * guard physical placement; TI protection granularity needs HW review.
 */
#pragma once
#include <stdint.h>
#define AM13E_FLASH_BOOT_BASE       UINT32_C(0x00000000)
#define AM13E_FLASH_BOOT_END        UINT32_C(0x00004000)
#define AM13E_FLASH_FW1_PARAM_BASE  UINT32_C(0x00004000)
#define AM13E_FLASH_FW1_PARAM_END   UINT32_C(0x00005000)
#define AM13E_FLASH_RESERVED_BASE   UINT32_C(0x00005000)
#define AM13E_FLASH_RESERVED_END    UINT32_C(0x00006000)
#define AM13E_FLASH_APP_BASE        UINT32_C(0x00006000)
#define AM13E_FLASH_APP_END         UINT32_C(0x00080000)
#define AM13E_FLASH_ERASE_SECTOR    UINT32_C(2048)
#define AM13E_FLASH_LOGICAL_BLOCK   UINT32_C(1024)
#define AM13E_FLASH_WINDOW_BLOCKS   UINT32_C(256)
#define AM13E_FLASH_APP_BYTES       (AM13E_FLASH_APP_END-AM13E_FLASH_APP_BASE)
#define AM13E_FLASH_APP_BLOCKS      (AM13E_FLASH_APP_BYTES/AM13E_FLASH_LOGICAL_BLOCK)
#define AM13E_FLASH_CFG_BLOCKS      ((AM13E_FLASH_FW1_PARAM_END-AM13E_FLASH_FW1_PARAM_BASE)/AM13E_FLASH_LOGICAL_BLOCK)
#define AM13E_FLASH_STREAM_BLOCKS   (AM13E_FLASH_CFG_BLOCKS+AM13E_FLASH_APP_BLOCKS)
_Static_assert(AM13E_FLASH_CFG_BLOCKS==4U && AM13E_FLASH_STREAM_BLOCKS==492U,
               "Rev1.6 Config-prefix logical stream geometry changed");
_Static_assert(AM13E_FLASH_APP_BLOCKS==488U &&
               AM13E_FLASH_ERASE_SECTOR==2U*AM13E_FLASH_LOGICAL_BLOCK &&
               AM13E_FLASH_FW1_PARAM_END==AM13E_FLASH_RESERVED_BASE &&
               AM13E_FLASH_RESERVED_END==AM13E_FLASH_APP_BASE,
               "AM13E Rev1.6 Boot/Config/Reserved/APP partition contract changed");
