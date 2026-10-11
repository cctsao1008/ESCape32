/* FW1 configuration Flash: deterministic, failure-reporting transaction
 * sequence. Does not provide crash atomicity; power loss during erase
 * invalidates the existing sector until a versioned journal is designed.
 * Pure C, allows fake Flash in host test and real TI driver callbacks later.
 */
#pragma once
#include "cfg_flash_plan.h"
#include <stdint.h>
typedef struct {
    /* Each operation must return nonzero ONLY on success. */
    int (*erase_sector)(uint32_t address, void *ctx);
    int (*program_ecc16)(uint32_t address, const uint32_t data[4], void *ctx);
    int (*read_ecc16)(uint32_t address, uint8_t out[16], void *ctx);
    void *ctx;
} AM13E_CfgFlashOps;
/* The source is supplied separately for host tests; FW1 hardware adapter
 * must first validate its true uintptr_t address via cfg_flash_plan().
 * Erase and program the whole required 2KiB sector(s), stage tail as
 * erased 0xFF, read-verify every 16-byte ECC unit and fail immediately
 * on ANY error. Never touch FW2 0x5000+.
 */
int am13e_cfg_flash_execute(const AM13E_CfgFlashPlan *plan,
                             const uint8_t *source,
                             const AM13E_CfgFlashOps *ops);
