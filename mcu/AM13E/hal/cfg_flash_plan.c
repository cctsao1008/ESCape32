#include "cfg_flash_plan.h"
#include "flash_partition.h"
#include <stdint.h>
#include <stddef.h>
#define FW1_CFG_FIRST AM13E_FLASH_FW1_PARAM_BASE
#define FW1_CFG_END AM13E_FLASH_FW1_PARAM_END
#define FW1_SECTOR_BYTES AM13E_FLASH_ERASE_SECTOR
#define FLASH_ECC_WRITE_BYTES UINT32_C(16)
#define RAM_S_FIRST UINT32_C(0x20000000)
#define RAM_S_END UINT32_C(0x20018000)
_Static_assert((FW1_CFG_END-FW1_CFG_FIRST)==2U*FW1_SECTOR_BYTES,
               "FW1 config partition must contain exactly two 2KiB sectors");
int am13e_cfg_flash_plan(uintptr_t destination, uintptr_t source,
                         uint32_t byte_count, AM13E_CfgFlashPlan *out)
{
    if (out==NULL || destination!=(uintptr_t)FW1_CFG_FIRST ||
        byte_count==0U || byte_count>(FW1_CFG_END-FW1_CFG_FIRST) ||
        source<(uintptr_t)RAM_S_FIRST || source>=(uintptr_t)RAM_S_END ||
        (uintptr_t)byte_count>((uintptr_t)RAM_S_END-source)) return 0;
    const uint32_t padded=(byte_count + (FLASH_ECC_WRITE_BYTES-1U)) &
                          ~(FLASH_ECC_WRITE_BYTES-1U);
    if (padded > (FW1_CFG_END-FW1_CFG_FIRST)) return 0;
    const AM13E_CfgFlashPlan plan={
        .destination=FW1_CFG_FIRST,
        .source=(uint32_t)source,
        .byte_count=byte_count,
        .padded_program_bytes=padded,
        .sector_count=(padded+FW1_SECTOR_BYTES-1U)/FW1_SECTOR_BYTES,
    };
    *out=plan;
    return 1;
}
