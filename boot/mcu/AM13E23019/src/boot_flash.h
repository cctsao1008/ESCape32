/*
 * AM13E23019 boot Flash backend contract.
 *
 * This wrapper keeps ESCape32 update semantics independent from TI DriverLib
 * details. Bank0 programming requires a RAM-resident transaction path and is
 * intentionally rejected by the normal DriverLib path for now.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOOT_FLASH_OK = 0,
    BOOT_FLASH_ERR_RANGE = -1,
    BOOT_FLASH_ERR_ALIGNMENT = -2,
    BOOT_FLASH_ERR_CROSS_BANK = -3,
    BOOT_FLASH_ERR_ACTIVE_BANK = -4,
    BOOT_FLASH_ERR_DRIVER = -5,
    BOOT_FLASH_ERR_VERIFY = -6,
} boot_flash_status_t;

bool boot_flash_app_range_valid(uint32_t address, uint32_t size);

boot_flash_status_t boot_flash_erase_sector(uint32_t address);
boot_flash_status_t boot_flash_program(
    uint32_t address, const uint8_t *data, uint32_t size);
boot_flash_status_t boot_flash_verify(
    uint32_t address, const uint8_t *data, uint32_t size);
