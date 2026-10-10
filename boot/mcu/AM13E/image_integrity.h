/*
 * AM13E reference/AM13E host-testable image integrity contract.
 *
 * v1.6 application contract: Cortex-M33 vectors at APP_BASE=0x6000.
 * Preserve the old 32-byte header/CRC algorithm and 1 KiB transport,
 * but move signature to APP+0x400 and header to APP+0x500, outside the
 * vector table and still within the first 2 KiB erase sector.
 * The legacy APP+0 signature/APP+0x800 vector format is INVALID.
 * Image marker/metadata offsets are explicit AM13E reference detailed-design choices. 
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../../mcu/AM13E/flash_partition.h"

/* Architectural addresses are fixed for AM13E reference v2. Host memory may be
 * mapped elsewhere, but the M33 Reset Handler remains a physical address.
 */
#define AM13E_IMAGE_APP_BASE            AM13E_FLASH_APP_BASE
#define AM13E_IMAGE_HEADER_OFFSET       0x500U
#define AM13E_IMAGE_HEADER_SIZE         32U
#define AM13E_IMAGE_SIGNATURE_OFFSET    0x400U
#define AM13E_IMAGE_VECTOR_OFFSET       0x000U
#define AM13E_IMAGE_METADATA_SECTOR     0x800U
#define AM13E_IMAGE_MAGIC               UINT32_C(0x49323645)
#define AM13E_IMAGE_TARGET              UINT32_C(0x33314d41)
#define AM13E_IMAGE_HEADER_VERSION      1U
#define AM13E_IMAGE_MAX_TRANSPORT_BYTES AM13E_FLASH_APP_BYTES

typedef enum {
    AM13E_IMAGE_INVALID_ARGUMENT = 0,
    AM13E_IMAGE_INVALID_HEADER,
    AM13E_IMAGE_INVALID_HEADER_CRC,
    AM13E_IMAGE_INVALID_LENGTH,
    AM13E_IMAGE_INVALID_SIGNATURE,
    AM13E_IMAGE_INVALID_VECTOR,
    AM13E_IMAGE_INVALID_PAYLOAD_CRC,
    AM13E_IMAGE_VALID
} boot_am13e_image_status_t;

/*
 * Validate an image in the mapped application Flash range [first,end).
 *
 * With prefix == NULL and prefix_len == 0, validate the committed Flash.
 * With prefix_len == 16, substitute APP+0x400's deferred 16-byte
 * signature program unit while the physical Flash marker is still erased.
 *
 * CRC-32/ISO-HDLC covers [0,image_length) EXCEPT the 32-byte
 * header at APP+0x500; header CRC covers its first 28 bytes.
 * Return the committed image length only when every check succeeds.
 */
boot_am13e_image_status_t boot_am13e_image_check(
    uintptr_t first, uintptr_t end, const uint8_t *prefix,
    unsigned prefix_len, uint32_t *validated_length);
