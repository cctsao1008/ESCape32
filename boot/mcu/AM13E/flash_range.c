/*
** AM13E boot Flash range validation.
** Hardware erasure/programming is implemented separately.
*/
#include "common.h"
#include <stdint.h>

extern char __boot_flash_end__[];
extern char __boot_storage_end__[];

static bool flash_range(unsigned block, unsigned length, uintptr_t *start) {
    const uintptr_t first = (uintptr_t)__boot_flash_end__;
    const uintptr_t limit = (uintptr_t)__boot_storage_end__;
    const uintptr_t offset = (uintptr_t)block * UINT32_C(1024);
    if (!length || (length & 3U) || limit <= first ||
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
    if (!address || !flash_range(block, length, &start) || (start & 7U) ||
        (length & 7U)) return false;
    *address = (char *)start;
    return true;
}
