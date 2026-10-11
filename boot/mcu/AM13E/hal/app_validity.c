/* Target-neutral Rel17 Cfg.id + relocated Cortex-M33 vector gate.
 * No signature, metadata, firmware length, or image CRC is required.
 * Boot cannot detect interrupted programming of later code bytes.
 */
#include "app_validity.h"
#include <stddef.h>
bool boot_am13e_app_validity(const void *cfg_flash,
                             const void *app_vectors,
                             uint32_t *initial_sp,
                             uint32_t *reset_pc)
{
    if(cfg_flash==NULL || app_vectors==NULL || initial_sp==NULL ||
       reset_pc==NULL) return false;
    const volatile uint16_t *id=(const volatile uint16_t *)cfg_flash;
    if (*id!=AM13E_BOOT_CFG_ID) return false;
    const volatile uint32_t *v=(const volatile uint32_t *)app_vectors;
    const uint32_t sp=v[0],pc=v[1];
    if((sp&7U)!=0U || sp<UINT32_C(0x20000008) ||
       sp>UINT32_C(0x20018000) || !(pc&1U))return false;
    const uint32_t entry=pc&~UINT32_C(1);
    if(entry<AM13E_FLASH_APP_BASE || entry>=AM13E_FLASH_APP_END)
        return false;
    *initial_sp=sp;
    *reset_pc=pc;
    return true;
}
