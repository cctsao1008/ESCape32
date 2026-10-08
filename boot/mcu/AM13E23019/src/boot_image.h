/*
 * AM13E23019 boot image policy.
 *
 * Architecture first: vector sanity is implemented now. Header/CRC/valid-record
 * policy is intentionally represented but not yet enforced.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BOOT_IMAGE_INVALID = 0,
    BOOT_IMAGE_VECTOR_VALID,
    BOOT_IMAGE_FULLY_VALID,
} boot_image_status_t;

boot_image_status_t boot_image_validate(void);
bool boot_image_launchable(void);
