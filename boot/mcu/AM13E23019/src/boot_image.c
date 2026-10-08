/*
 * AM13E23019 boot image policy.
 */

#include "boot_image.h"

#include <stddef.h>
#include <stdint.h>

#include "boot_port.h"
#include "crc32.h"
#include "image_contract.h"

static bool boot_image_header_fields_valid(
    const escape32_image_header_t *header)
{
    if (header->magic != ESCAPE32_IMAGE_HEADER_MAGIC) {
        return false;
    }

    if ((header->header_version != ESCAPE32_IMAGE_HEADER_VERSION) ||
        (header->header_size != ESCAPE32_IMAGE_HEADER_SIZE)) {
        return false;
    }

    if (header->target_id != ESCAPE32_IMAGE_TARGET_AM13E23019) {
        return false;
    }

    if ((header->image_length < ESCAPE32_IMAGE_HEADER_END -
            ESCAPE32_APP_BASE) ||
        (header->image_length > ESCAPE32_APP_SIZE) ||
        ((header->image_length & 0x0fU) != 0U)) {
        return false;
    }

    if ((header->flags != ESCAPE32_IMAGE_FLAG_NONE) ||
        (header->reserved != 0U)) {
        return false;
    }

    return true;
}

static bool boot_image_header_crc_valid(
    const escape32_image_header_t *header)
{
    uint32_t crc = boot_crc32(
        header,
        (uint32_t)offsetof(escape32_image_header_t, header_crc32));

    return crc == header->header_crc32;
}

static uint32_t boot_image_payload_crc(
    const escape32_image_header_t *header)
{
    const uint8_t *image =
        (const uint8_t *)(uintptr_t)ESCAPE32_APP_BASE;

    uint32_t state = boot_crc32_begin();

    state = boot_crc32_update(
        state,
        image,
        ESCAPE32_IMAGE_HEADER_OFFSET);

    uint32_t tail_offset =
        ESCAPE32_IMAGE_HEADER_OFFSET + ESCAPE32_IMAGE_HEADER_SIZE;
    uint32_t tail_length = header->image_length - tail_offset;

    state = boot_crc32_update(
        state,
        image + tail_offset,
        tail_length);

    return boot_crc32_end(state);
}

boot_image_status_t boot_image_validate(void)
{
    if (!boot_port_app_valid()) {
        return BOOT_IMAGE_INVALID_VECTOR;
    }

    const escape32_image_header_t *header =
        (const escape32_image_header_t *)(uintptr_t)
            ESCAPE32_IMAGE_HEADER_BASE;

    if (!boot_image_header_fields_valid(header)) {
        return BOOT_IMAGE_INVALID_HEADER;
    }

    if (!boot_image_header_crc_valid(header)) {
        return BOOT_IMAGE_INVALID_HEADER_CRC;
    }

    if (boot_image_payload_crc(header) != header->image_crc32) {
        return BOOT_IMAGE_INVALID_IMAGE_CRC;
    }

    return BOOT_IMAGE_FULLY_VALID;
}

bool boot_image_launchable(void)
{
    return boot_image_validate() == BOOT_IMAGE_FULLY_VALID;
}
