/*
** AM13E application Flash programming using TI DriverLib.
** WARNING: Bank 0 cannot be erased while code executes from Bank 0.
** This backend rejects Bank 0 operations until a qualified RAM-resident
** flash service is available. The complete rel17 update protocol must
** account for 2 KiB sectors and 1 KiB transport blocks.
*/
#include "common.h"
#include <dl_flash.h>
#include <stdint.h>

extern char __app_flash_start__[];
extern char __boot_storage_end__[];

int boot_am13e_flash_write(char *dst, const char *src, int len) {
    uintptr_t addr = (uintptr_t)dst;
    uintptr_t end = (uintptr_t)__boot_storage_end__;
    uintptr_t first = (uintptr_t)__app_flash_start__;
    if (!src || len <= 0 || (addr & 15U) != 0U ||
        ((unsigned)len & 15U) != 0U || addr < first ||
        addr >= end || (unsigned)len > end - addr ||
        (unsigned)len > 1024U)
        return 0;

    /* Flash operations targeting the executing bank are unsupported.
     * Do not bypass this until the full erase/program call chain is
     * linked in RAM and validated on hardware.
     */
    if ((addr / DL_FLASH_BANK_SIZE) ==
        ((uintptr_t)&boot_am13e_flash_write / DL_FLASH_BANK_SIZE))
        return 0;

    /* Only erase on the first 1 KiB block of each 2 KiB sector.
     * The host protocol MUST transmit ordered blocks; an interruption
     * between blocks can leave an incomplete application image.
     */
    if ((addr % DL_FLASH_SECTOR_SIZE) == 0U) {
        if (DL_Flash_eraseSector((uint32_t)addr) != DL_FLASH_SUCCESS)
            return 0;
    }

    if (DL_Flash_program((uint32_t)addr, (uint8_t *)(uintptr_t)src,
                         (uint32_t)len) != DL_FLASH_SUCCESS)
        return 0;

    const volatile uint8_t *verify = (const volatile uint8_t *)addr;
    for (int i = 0; i < len; ++i) {
        if (verify[i] != (uint8_t)src[i])
            return 0;
    }
    return 1;
}
