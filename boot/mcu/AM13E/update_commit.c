/* Rel17 v1.4: Boot Bank0 self-update sector execution experiment.
 *
 * This is NOT wired to CMD_UPDATE. Boot self-update remains unavailable
 * until physical recovery, post-erase SRAM call-graph, watchdog/IRQ,
 * NONMAIN and power-fail qualification are demonstrated.
 *
 * Original recvdata() + per-frame CRC/ACK is in boot/src, while
 * update_staging.c reserves 16KiB in SRAM_S. Each run erases/programs
 * eight 2KiB MAIN sectors only in BOOT [0,0x4000), verifies each
 * byte, and leaves Config/Reserved/APP wholly untouched.
 *
 * No new image format, checksum or transport command is introduced.
 */
#include "update_commit.h"
#include "update_staging.h"
#include <dl_flash.h>
#include <stdint.h>
#ifndef AM13E_BOOT_COMMIT_HOST_TEST
#include <soc.h>
#endif

_Static_assert(AM13E_FLASH_BOOT_BASE==UINT32_C(0x00000000),
               "Rev1.4 Boot starts at MAIN Bank0 0");
_Static_assert(AM13E_BOOT_STAGE_BYTES==8U*AM13E_FLASH_ERASE_SECTOR,
               "Boot commit is exactly eight 2KiB sectors");
_Static_assert((AM13E_FLASH_BOOT_END & 15U)==0U,
               "ECC write boundary must be 16B aligned");

/* The ENTIRE post-erase loop, verification and return to its caller
 * runs from SRAM_C. The ARM wrapper below is also SRAM_C and NEVER
 * returns to erased Flash. TI dl_flash/dl_flashctl .text is placed
 * in SRAM_C by the same Boot config.ld contract.
 *
 * This is source/link evidence only: disassembly and on-device
 * bank-conflict/recovery tests are still required.
 */
__attribute__((section(".TI.ramfunc"),noinline,used))
static AM13E_BootCommitStatus boot_am13e_commit_sectors(
    uint32_t boot_base,uint8_t *image)
{
    if(image==0 || (boot_base & (AM13E_FLASH_ERASE_SECTOR-1U))!=0U)
        return AM13E_BOOT_COMMIT_BAD_IMAGE;

    for(uint32_t offset=0U;offset<AM13E_BOOT_STAGE_BYTES;
        offset+=AM13E_FLASH_ERASE_SECTOR){
        const uint32_t address=boot_base+offset;
        if(DL_Flash_eraseSector(address)!=DL_FLASH_SUCCESS)
            return AM13E_BOOT_COMMIT_ERASE_FAILED;
        if(DL_Flash_program(address,image+offset,
                            AM13E_FLASH_ERASE_SECTOR)!=DL_FLASH_SUCCESS)
            return AM13E_BOOT_COMMIT_PROGRAM_FAILED;
        const volatile uint8_t *actual=
            (const volatile uint8_t *)(uintptr_t)address;
        for(uint32_t byte=0U;byte<AM13E_FLASH_ERASE_SECTOR;++byte)
            if(actual[byte]!=image[offset+byte])
                return AM13E_BOOT_COMMIT_VERIFY_FAILED;
    }
    return AM13E_BOOT_COMMIT_OK;
}

#ifdef AM13E_BOOT_COMMIT_HOST_TEST
AM13E_BootCommitStatus boot_am13e_update_commit_host_run(
    uint32_t test_boot_base)
{
    /* Exercise the exact sector algorithm against a bounded Host
     * Flash mock; this is not an on-target boot authorization.
     */
    uint8_t *image=boot_am13e_stage_prepare_for_commit();
    if(image==0) return AM13E_BOOT_COMMIT_BAD_IMAGE;
    return boot_am13e_commit_sectors(test_boot_base,image);
}
#else
/* This nonreturning entry is intentionally unreachable from the
 * production Boot command dispatcher until recovery is qualified.
 * Only stage preparation executes from Flash; after IRQ masking,
 * the call, return, verification and reset/halt path are all RAM_C.
 */
__attribute__((section(".TI.ramfunc"),noinline,noreturn,used))
void boot_am13e_update_commit_quarantined(void)
{
    uint8_t *image=boot_am13e_stage_prepare_for_commit();
    if(image==0) {
        for(;;) { __NOP(); }
    }

    __disable_irq();
    __DSB();__ISB();
    const AM13E_BootCommitStatus result=
        boot_am13e_commit_sectors(AM13E_FLASH_BOOT_BASE,image);
    if(result==AM13E_BOOT_COMMIT_OK)
        NVIC_SystemReset();
    /* Failed erasure/programming must NEVER return into Bank0 Boot.
     * Recovery may only be attempted through hardware-qualified paths.
     */
    for(;;) { __NOP(); }
}
#endif
