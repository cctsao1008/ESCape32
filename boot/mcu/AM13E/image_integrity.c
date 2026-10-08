/*
 * E62 32-byte image metadata and whole-image CRC validator.
 * Portable C: no TI DriverLib or target-specific headers.
 */
#include "image_integrity.h"

static uint16_t le16(const volatile uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t le32(const volatile uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t crc_update(uint32_t state, uint8_t byte) {
    state ^= byte;
    for (unsigned bit = 0; bit < 8U; ++bit) {
        uint32_t mask = 0U - (state & UINT32_C(1));
        state = (state >> 1) ^ (UINT32_C(0xedb88320) & mask);
    }
    return state;
}
static uint32_t header_crc(const volatile uint8_t *header) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (unsigned i = 0; i < 28U; ++i)
        crc = crc_update(crc, header[i]);
    return ~crc;
}

boot_am13e_image_status_t boot_am13e_image_check(
    uintptr_t first, uintptr_t end, const uint8_t *prefix,
    unsigned prefix_len, uint32_t *validated_length) {
    if (validated_length) *validated_length = 0U;
    if (end <= first || (end - first) <
        AM13E_IMAGE_VECTOR_OFFSET + 8U ||
        (prefix_len != 0U && prefix_len != 16U) ||
        ((prefix_len == 0U) != (prefix == 0)))
        return AM13E_IMAGE_INVALID_ARGUMENT;

    const volatile uint8_t *image = (const volatile uint8_t *)first;
    const volatile uint8_t *header = image + AM13E_IMAGE_HEADER_OFFSET;
    if (le32(header) != AM13E_IMAGE_MAGIC ||
        le16(header + 4) != AM13E_IMAGE_HEADER_VERSION ||
        le16(header + 6) != AM13E_IMAGE_HEADER_SIZE ||
        le32(header + 8) != AM13E_IMAGE_TARGET ||
        le32(header + 20) != 0U || le32(header + 24) != 0U)
        return AM13E_IMAGE_INVALID_HEADER;

    if (le32(header + 28) != header_crc(header))
        return AM13E_IMAGE_INVALID_HEADER_CRC;

    const uint32_t len = le32(header + 12);
    if (len < AM13E_IMAGE_VECTOR_OFFSET + 16U ||
        len > AM13E_IMAGE_MAX_TRANSPORT_BYTES ||
        len > end - first || (len & 15U) != 0U)
        return AM13E_IMAGE_INVALID_LENGTH;

    const uint8_t signature0 = prefix_len ? prefix[0] : image[0];
    const uint8_t signature1 = prefix_len ? prefix[1] : image[1];
    if (signature0 != UINT8_C(0xea) || signature1 != UINT8_C(0x32))
        return AM13E_IMAGE_INVALID_SIGNATURE;

    const uint32_t sp = le32(image + AM13E_IMAGE_VECTOR_OFFSET);
    const uint32_t pc = le32(image + AM13E_IMAGE_VECTOR_OFFSET + 4U);
    const uintptr_t entry = (uintptr_t)(pc & ~UINT32_C(1));
    if ((sp & 7U) || sp < UINT32_C(0x20000008) ||
        sp > UINT32_C(0x20018000) || (pc & 1U) == 0U ||
        entry < first + AM13E_IMAGE_VECTOR_OFFSET ||
        entry >= first + len)
        return AM13E_IMAGE_INVALID_VECTOR;

    uint32_t crc = UINT32_C(0xffffffff);
    for (uint32_t i = 0U; i < len; ++i) {
        if (i >= AM13E_IMAGE_HEADER_OFFSET &&
            i < AM13E_IMAGE_HEADER_OFFSET + AM13E_IMAGE_HEADER_SIZE)
            continue;
        const uint8_t value =
            (prefix_len && i < prefix_len) ? prefix[i] : image[i];
        crc = crc_update(crc, value);
    }
    if (~crc != le32(header + 16))
        return AM13E_IMAGE_INVALID_PAYLOAD_CRC;

    if (validated_length) *validated_length = len;
    return AM13E_IMAGE_VALID;
}
