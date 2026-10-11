#if defined(AM13E) || defined(AM13E_FLASH_TEST) || defined(AM13E_BOOT_STAGE_TEST)
/*
** AM13E boot Flash range validation.
** Hardware erasure/programming is implemented separately.
*/
#include "common.h"
#include <stdint.h>
#include "../../mcu/AM13E/hal/flash_partition.h"

#ifdef AM13E_FLASH_TEST
/* Host test-only: use the same mapped Flash bounds as flash.c. */
extern uintptr_t boot_am13e_test_first;
extern uintptr_t boot_am13e_test_end;
#define BOOT_APP_FIRST boot_am13e_test_first
#define BOOT_APP_END boot_am13e_test_end
#else
extern char __app_flash_start__[];
extern char __boot_storage_end__[];
#define BOOT_APP_FIRST ((uintptr_t)__app_flash_start__)
#define BOOT_APP_END ((uintptr_t)__boot_storage_end__)
#endif

static bool flash_range(unsigned block, unsigned length, uintptr_t *start) {
    const uintptr_t app = BOOT_APP_FIRST;
    const uintptr_t limit = BOOT_APP_END;
    if (app < AM13E_FLASH_APP_BASE-AM13E_FLASH_FW1_PARAM_BASE ||
        block >= AM13E_FLASH_STREAM_BLOCKS) return false;
    const bool config = block < AM13E_FLASH_CFG_BLOCKS;
    const uintptr_t first = config ?
        app-(AM13E_FLASH_APP_BASE-AM13E_FLASH_FW1_PARAM_BASE) : app;
    const uintptr_t end = config ? first+AM13E_FLASH_CFG_BLOCKS*1024U : limit;
    const uintptr_t offset = (uintptr_t)(config ? block :
        block-AM13E_FLASH_CFG_BLOCKS)*UINT32_C(1024);
    if (!length || length > AM13E_FLASH_LOGICAL_BLOCK || (length & 3U) || end <= first ||
        offset > end - first ||
        (uintptr_t)length > end - first - offset)
        return false;
    *start = first + offset;
    return true;
}

bool boot_am13e_read_range(unsigned block, unsigned length, const void **address) {
    uintptr_t start;
    if (!address || !flash_range(block, length, &start)) return false;
    *address = (const void *)start;
    return true;
}

bool boot_am13e_write_range(unsigned block, unsigned length, char **address) {
    uintptr_t start;
    if (!address || !flash_range(block, length, &start) ||
        (start & 15U)) return false;
    if ((length & 3U) != 0U) return false;
    *address = (char *)start;
    return true;
}

#else
typedef int am13e_inactive_translation_unit;
#endif
