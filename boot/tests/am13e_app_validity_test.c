/* Rel17 v1.4 native boot gate: Cfg.id at 0x4000 and APP M33 vectors.
 * No image header/CRC/special marker. Host mapping does not change the
 * physical Reset Handler PC range used by the device.
 */
#include "app_validity.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    uint16_t cfg[8]={AM13E_BOOT_CFG_ID,0U};
    uint32_t vec[2]={UINT32_C(0x20018000),AM13E_FLASH_APP_BASE+0x101U};
    uint32_t sp=0U,pc=0U;
    assert(boot_am13e_app_validity(cfg,vec,&sp,&pc));
    assert(sp==UINT32_C(0x20018000) && pc==AM13E_FLASH_APP_BASE+0x101U);
    /* No requirement for APP+0x400/0x500 or a fixed-size image. */
    cfg[0]=0xffffU;
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    cfg[0]=AM13E_BOOT_CFG_ID;
    vec[0]=UINT32_C(0xffffffff);
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[0]=UINT32_C(0x20018000);
    vec[1]=UINT32_C(0xffffffff);
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[1]=AM13E_FLASH_APP_BASE-1U;
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[1]=AM13E_FLASH_APP_END+1U;
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[1]=AM13E_FLASH_APP_END-1U;
    assert(boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[1]=AM13E_FLASH_APP_BASE+1U;
    assert(boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[0]=UINT32_C(0x20000003);
    assert(!boot_am13e_app_validity(cfg,vec,&sp,&pc));
    vec[0]=UINT32_C(0x20000100);
    assert(boot_am13e_app_validity(cfg,vec,&sp,&pc));
    assert(!boot_am13e_app_validity(NULL,vec,&sp,&pc));
    assert(!boot_am13e_app_validity(cfg,NULL,&sp,&pc));
    assert(!boot_am13e_app_validity(cfg,vec,NULL,&pc));
    assert(!boot_am13e_app_validity(cfg,vec,&sp,NULL));
    puts("PASS original Rel17 0x32EA Cfg.id + M33 APP_BASE vectors (no CRC/marker)");
    return 0;
}
