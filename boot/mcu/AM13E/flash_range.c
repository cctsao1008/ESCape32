/*
** AM13E boot Flash range validation.
** Hardware erasure/programming is implemented separately.
*/
#include "common.h"
#include <stdint.h>

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
    const uintptr_t first = BOOT_APP_FIRST;
    const uintptr_t limit = BOOT_APP_END;
    const uintptr_t offset = (uintptr_t)block * UINT32_C(1024);
    if (!length || length > 1024U || (length & 3U) || block > 255U || limit <= first ||
        block > UINT32_MAX / UINT32_C(1024) ||
        offset > limit - first ||
        (uintptr_t)length > limit - first - offset)
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
    /* WiFi-Link invalidates the image with two eight-byte 0xff writes.
     * These are handled as a special erase transaction in flash.c.
     */
    if ((length & 3U) != 0U) return false;
    if (length == 8U && block > 1U) return false;
    *address = (char *)start;
    return true;
}
