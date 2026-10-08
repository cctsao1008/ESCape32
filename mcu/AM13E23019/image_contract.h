/*
 * E62 AM13E23019 application image contract.
 *
 * The application vector remains fixed at ESCAPE32_APP_BASE. A 32-byte image
 * header is reserved immediately after the 0x100-byte vector window so normal
 * ESCape32 block writes can carry the header without padding the image to the
 * end of MAIN Flash.
 */
#pragma once

#include <stdint.h>

#include "flash_layout.h"

#define ESCAPE32_IMAGE_HEADER_OFFSET       (0x00000100UL)
#define ESCAPE32_IMAGE_HEADER_BASE             (ESCAPE32_APP_BASE + ESCAPE32_IMAGE_HEADER_OFFSET)
#define ESCAPE32_IMAGE_HEADER_SIZE         (0x00000020UL)
#define ESCAPE32_IMAGE_HEADER_END              (ESCAPE32_IMAGE_HEADER_BASE + ESCAPE32_IMAGE_HEADER_SIZE)

#define ESCAPE32_IMAGE_HEADER_MAGIC        (0x49323645UL) /* "E62I" */
#define ESCAPE32_IMAGE_HEADER_VERSION      (1U)
#define ESCAPE32_IMAGE_TARGET_AM13E23019   (0x33314D41UL) /* "AM13" */

#define ESCAPE32_IMAGE_FLAG_NONE           (0x00000000UL)

/*
 * CRC-32/ISO-HDLC:
 *   reflected polynomial 0xEDB88320
 *   init 0xFFFFFFFF
 *   xorout 0xFFFFFFFF
 *
 * image_crc32 covers [APP_BASE, APP_BASE + image_length), excluding the
 * 32-byte image header itself. header_crc32 covers the first 28 bytes of the
 * header.
 */
typedef struct {
    uint32_t magic;
    uint16_t header_version;
    uint16_t header_size;
    uint32_t target_id;
    uint32_t image_length;
    uint32_t image_crc32;
    uint32_t flags;
    uint32_t reserved;
    uint32_t header_crc32;
} escape32_image_header_t;

_Static_assert(sizeof(escape32_image_header_t) == ESCAPE32_IMAGE_HEADER_SIZE,
    "E62 image header must remain 32 bytes");
_Static_assert((ESCAPE32_IMAGE_HEADER_BASE & 0x0fUL) == 0UL,
    "E62 image header must be 16-byte aligned");
_Static_assert(ESCAPE32_IMAGE_HEADER_END < ESCAPE32_APP_END,
    "E62 image header must live inside the APP region");
