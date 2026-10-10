#include "cfg_flash_writer.h"
#include "flash_partition.h"
#include <stddef.h>
#include <stdint.h>
#define FW1_CFG_ADDRESS AM13E_FLASH_FW1_PARAM_BASE
#define FW1_CFG_LIMIT AM13E_FLASH_FW1_PARAM_END
#define FLASH_SECTOR_SIZE AM13E_FLASH_ERASE_SECTOR
#define ECC_SIZE UINT32_C(16)
int am13e_cfg_flash_execute(const AM13E_CfgFlashPlan *plan,
                             const uint8_t *source,
                             const AM13E_CfgFlashOps *ops)
{
    if (plan==NULL || source==NULL || ops==NULL ||
        ops->erase_sector==NULL || ops->program_ecc16==NULL ||
        ops->read_ecc16==NULL || plan->destination!=FW1_CFG_ADDRESS ||
        plan->byte_count==0U || plan->byte_count>4096U ||
        plan->padded_program_bytes<plan->byte_count ||
        plan->padded_program_bytes>4096U ||
        (plan->padded_program_bytes & 15U)!=0U ||
        /* Never permit caller-controlled over-erasure. Exactly
         * ceil(byte_count/16)*16 bytes are programmed, no more.
         */
        plan->padded_program_bytes !=
          ((plan->byte_count+ECC_SIZE-1U) & ~(ECC_SIZE-1U)) ||
        plan->sector_count==0U || plan->sector_count>2U ||
        plan->sector_count !=
          (plan->padded_program_bytes+FLASH_SECTOR_SIZE-1U)/FLASH_SECTOR_SIZE)
        return 0;

    /* There is intentionally no all-at-once transaction guarantee here.
     * The real FW1 caller must quiesce the bridge, freeze ISR consumers
     * of this Flash bank and establish a versioned storage protocol.
     * This engine never claims rollback after interrupted erasure.
     */
    for(uint32_t sector=0U; sector<plan->sector_count; ++sector) {
        const uint32_t base=FW1_CFG_ADDRESS+sector*FLASH_SECTOR_SIZE;
        if (base>=FW1_CFG_LIMIT ||
            !ops->erase_sector(base,ops->ctx)) return 0;
        const uint32_t end=plan->padded_program_bytes <
             (sector+1U)*FLASH_SECTOR_SIZE ?
             plan->padded_program_bytes : (sector+1U)*FLASH_SECTOR_SIZE;
        for(uint32_t offset=sector*FLASH_SECTOR_SIZE;
            offset<end;offset+=ECC_SIZE) {
            /* Word-aligned SRAM staging: do not read beyond source length.
             * ECC padding must be erased 0xFF, never unrelated RAM bytes.
             */
            uint32_t staged[4]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
            uint8_t *bytes=(uint8_t*)staged;
            for(uint32_t k=0U;k<ECC_SIZE;++k)
                if (offset+k<plan->byte_count)
                    bytes[k]=source[offset+k];

            const uint32_t address=FW1_CFG_ADDRESS+offset;
            if (address>=FW1_CFG_LIMIT ||
                !ops->program_ecc16(address,staged,ops->ctx)) return 0;
            uint8_t readback[16];
            if (!ops->read_ecc16(address,readback,ops->ctx)) return 0;
            for(uint32_t k=0U;k<ECC_SIZE;++k)
                if (readback[k]!=bytes[k]) return 0;
        }
    }
    return 1;
}
