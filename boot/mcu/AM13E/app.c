/*
 * ESCape32 rel17 v1.4: original Cfg.id==0x32EA launch gate.
 * APP_BASE=0x6000 is a relocated native M33 vector table.
 * No v1.6 signature, CRC, header, or fixed image size requirement.
 */
#include "common.h"
#include <stdint.h>
#include <soc.h>
#include "app_validity.h"

extern char __cfg_flash_start__[];
extern char __app_flash_start__[];
extern char __app_vector_start__[];
extern char __boot_storage_end__[];

static bool app_entry(uint32_t *sp,uint32_t *pc)
{
    const uintptr_t first=(uintptr_t)__app_flash_start__;
    const uintptr_t vec=(uintptr_t)__app_vector_start__;
    const uintptr_t end=(uintptr_t)__boot_storage_end__;
    if(first!=AM13E_FLASH_APP_BASE ||
       vec!=first || (vec&127U)!=0U || end!=AM13E_FLASH_APP_END ||
       (uintptr_t)__cfg_flash_start__!=AM13E_FLASH_FW1_PARAM_BASE)
        return false;
    return boot_am13e_app_validity(__cfg_flash_start__,
                                   __app_vector_start__,sp,pc);
}
bool boot_am13e_application_valid(void)
{
    uint32_t sp=0U,pc=0U;
    return app_entry(&sp,&pc);
}
__attribute__((noreturn)) void boot_am13e_launch_application(void)
{
    uint32_t sp=0U,pc=0U;
    if(!app_entry(&sp,&pc))for(;;){}
    __disable_irq();
    SysTick->CTRL=0U;
    SysTick->LOAD=0U;
    SysTick->VAL=0U;
    SCB->VTOR=(uint32_t)(uintptr_t)__app_vector_start__;
    __DSB();__ISB();
    __set_MSP(sp);
    __asm__ volatile ("bx %0" :: "r"(pc) : "memory");
    __builtin_unreachable();
}
