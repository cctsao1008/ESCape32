/*
 * AM13E23019 boot image policy.
 */

#include "boot_image.h"

#include "boot_port.h"

boot_image_status_t boot_image_validate(void)
{
    if (!boot_port_app_valid()) {
        return BOOT_IMAGE_INVALID;
    }

    /*
     * PSEUDOCODE / detailed design:
     *
     * header = read APP image record
     * if header.magic/version/target invalid:
     *     return BOOT_IMAGE_INVALID
     *
     * if header.length exceeds APP region:
     *     return BOOT_IMAGE_INVALID
     *
     * if crc32(APP payload) != header.crc32:
     *     return BOOT_IMAGE_INVALID
     *
     * if valid-record indicates incomplete update:
     *     return BOOT_IMAGE_INVALID
     *
     * return BOOT_IMAGE_FULLY_VALID
     *
     * Until that record format is frozen, vector sanity is the software
     * baseline and remains launchable for bring-up.
     */
    return BOOT_IMAGE_VECTOR_VALID;
}

bool boot_image_launchable(void)
{
    return boot_image_validate() != BOOT_IMAGE_INVALID;
}
