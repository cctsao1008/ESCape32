/*
 * E62/AM13E host-testable image integrity contract.
 *
 * This reuses the prior E62 32-byte header field layout and CRC algorithm,
 * but NOT its vector-first binary layout. The current v2 boot architecture
 * keeps ESCape32 signature at APP+0 and the M33 vector table at APP+0x800.
 * Do not use an old vector-first .e62.bin with this validator.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Architectural addresses are fixed for E62 v2. Host memory may be
 * mapped elsewhere, but the M33 Reset Handler remains a physical address.
 */
#define AM13E_IMAGE_APP_BASE            UINT32_C(0x00006000)
#define AM13E_IMAGE_HEADER_OFFSET       0x100U
#define AM13E_IMAGE_HEADER_SIZE         32U
#define AM13E_IMAGE_VECTOR_OFFSET       0x800U
#define AM13E_IMAGE_MAGIC               UINT32_C(0x49323645)
#define AM13E_IMAGE_TARGET              UINT32_C(0x33314d41)
#define AM13E_IMAGE_HEADER_VERSION      1U
#define AM13E_IMAGE_MAX_TRANSPORT_BYTES (256U * 1024U)

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
 * With prefix_len == 16, substitute the pending first 16 bytes while the
 * signature program unit remains erased (signature-last transaction).
 *
 * The v1 E62 CRC-32/ISO-HDLC covers [0,image_length) EXCEPT the 32-byte
 * image header at +0x100. The header CRC covers its first 28 bytes.
 * Return the committed image length only when every check succeeds.
 */
boot_am13e_image_status_t boot_am13e_image_check(
    uintptr_t first, uintptr_t end, const uint8_t *prefix,
    unsigned prefix_len, uint32_t *validated_length);
