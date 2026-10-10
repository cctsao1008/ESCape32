/* AM13E FW1 configuration-flash partition and ECC alignment plan.
 * Pure address arithmetic; never erases or programs physical Flash.
 * Current production contract: FW1 config 0x4000..0x4fff; FW2 begins 0x5000.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t destination;
    uint32_t source;
    uint32_t byte_count;
    uint32_t padded_program_bytes;
    uint32_t sector_count;
} AM13E_CfgFlashPlan;
/* Accept only the fixed FW1 settings origin and a source span entirely
 * within the application's RAM_S working SRAM. This intentionally rejects
 * pointers in Boot, FW2, application code or unknown memory ranges.
 * Source bytes beyond byte_count must never be read by a Flash backend.
 */
int am13e_cfg_flash_plan(uintptr_t destination, uintptr_t source,
                         uint32_t byte_count, AM13E_CfgFlashPlan *out);
