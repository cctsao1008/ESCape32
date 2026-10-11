/* Original setwrp(0=Off,1=Boot,2=Full), TI NONMAIN bank0 NM1.
 * Compiles destructive operations ONLY for explicitly recovery-qualified
 * boards: ROM BSL/SWD and NONMAIN rollback + brownout tested.
 * Reference CMake has no qualification and remains NAK-only.
 */
#include "wrp_nonmain.h"
#include <stdint.h>
#if defined(AM13E_BOOT_RECOVERY_QUALIFIED) && AM13E_BOOT_RECOVERY_QUALIFIED
#if !defined(AM13E_BOOT_ROM_BSL_SWD_RECOVERY_TESTED) || \
    !defined(AM13E_BOOT_NONMAIN_REVERSIBILITY_TESTED) || \
    !defined(AM13E_BOOT_POWERLOSS_RECOVERY_TESTED) || \
    (AM13E_BOOT_ROM_BSL_SWD_RECOVERY_TESTED!=1) || \
    (AM13E_BOOT_NONMAIN_REVERSIBILITY_TESTED!=1) || \
    (AM13E_BOOT_POWERLOSS_RECOVERY_TESTED!=1)
#error "Boot NONMAIN WRP requires verified ROM BSL/SWD, reversibility and power-loss recovery"
#endif
#include <soc.h>
#include <dl_flash.h>
#include <dl_flashctl.h>
static uint8_t planned[AM13E_BOOT_NONMAIN_CFG_BYTES]
    __attribute__((aligned(16)));
/* Source and FlashCTL DriverLib are SRAM_C resident once erase begins.
 * No return into Bank0 Boot is allowed after a failed NONMAIN erase.
 */
__attribute__((section(".TI.ramfunc"),noinline,noreturn,used))
static void commit_nonmain_sram(void)
{
    __disable_irq();__DSB();__ISB();
    if(DL_FlashCTL_acquireFlashSemaphore()!=DL_FLASH_SUCCESS)
        for(;;){__NOP();}
    DL_FlashCTL_executeClearStatus(NVMNW);
    DL_FlashCTL_unprotectNonMainMemory(NVMNW);
    if(DL_FlashCTL_eraseMemory(NVMNW,AM13E_BOOT_NONMAIN_CFG_ADDR,
                              DL_FLASHCTL_COMMAND_SIZE_SECTOR)!=
       DL_FLASHCTL_COMMAND_STATUS_PASSED)
        for(;;){__NOP();}
    for(unsigned pos=0U;pos<AM13E_BOOT_NONMAIN_CFG_BYTES;pos+=16U){
        DL_FlashCTL_executeClearStatus(NVMNW);
        DL_FlashCTL_unprotectNonMainMemory(NVMNW);
        if(DL_FlashCTL_programMemory128WithECCGenerated(
               NVMNW,AM13E_BOOT_NONMAIN_CFG_ADDR+pos,
               (const uint32_t *)(const void *)(planned+pos))!=
           DL_FLASHCTL_COMMAND_STATUS_PASSED)
            for(;;){__NOP();}
        if(DL_FlashCTL_readVerify128WithECCGenerated(
               NVMNW,AM13E_BOOT_NONMAIN_CFG_ADDR+pos,
               (const uint32_t *)(const void *)(planned+pos))!=
           DL_FLASHCTL_COMMAND_STATUS_PASSED)
            for(;;){__NOP();}
    }
    (void)DL_FlashCTL_releaseFlashSemaphore();
    NVIC_SystemReset(); /* boot-time static latch, POR test pending */
    for(;;){__NOP();}
}
#endif
void setwrp(int type)
{
#if defined(AM13E_BOOT_RECOVERY_QUALIFIED) && AM13E_BOOT_RECOVERY_QUALIFIED
    if(type<0||type>2)return;
    bool dirty=false;
    const uint8_t *existing=
        (const uint8_t *)(uintptr_t)AM13E_BOOT_NONMAIN_CFG_ADDR;
    if(!boot_am13e_nonmain_wrp_prepare(existing,
          AM13E_BOOT_NONMAIN_CFG_BYTES,(AM13E_NonmainWrpMode)type,
          planned,sizeof planned,&dirty))return;
    if(!dirty){NVIC_SystemReset();for(;;){__NOP();}}
    commit_nonmain_sram();
#else
    (void)type; /* physically NONMAIN-read-only reference */
#endif
}
