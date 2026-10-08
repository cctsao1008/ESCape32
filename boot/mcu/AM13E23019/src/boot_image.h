/*
 * AM13E23019 boot image policy.
 *
 * A launchable application requires both a sane Cortex-M33 vector table and a
 * valid E62 image header/CRC record.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BOOT_IMAGE_INVALID_VECTOR = 0,
    BOOT_IMAGE_INVALID_HEADER,
    BOOT_IMAGE_INVALID_HEADER_CRC,
    BOOT_IMAGE_INVALID_IMAGE_CRC,
    BOOT_IMAGE_FULLY_VALID,
} boot_image_status_t;

boot_image_status_t boot_image_validate(void);
bool boot_image_launchable(void);
