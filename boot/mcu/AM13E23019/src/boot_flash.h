/*
 * E62 AM13E23019 boot Flash backend contract.
 *
 * This wrapper keeps E62/ESCape32 update semantics independent from TI
 * DriverLib details.  Bank0 programming requires a RAM-resident transaction
 * path and is intentionally rejected by the normal DriverLib path for now.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    E62_BOOT_FLASH_OK = 0,
    E62_BOOT_FLASH_ERR_RANGE = -1,
    E62_BOOT_FLASH_ERR_ALIGNMENT = -2,
    E62_BOOT_FLASH_ERR_CROSS_BANK = -3,
    E62_BOOT_FLASH_ERR_ACTIVE_BANK = -4,
    E62_BOOT_FLASH_ERR_DRIVER = -5,
    E62_BOOT_FLASH_ERR_VERIFY = -6,
} e62_boot_flash_status_t;

bool e62_boot_flash_app_range_valid(uint32_t address, uint32_t size);

e62_boot_flash_status_t e62_boot_flash_erase_sector(uint32_t address);
e62_boot_flash_status_t e62_boot_flash_program(
    uint32_t address, const uint8_t *data, uint32_t size);
e62_boot_flash_status_t e62_boot_flash_verify(
    uint32_t address, const uint8_t *data, uint32_t size);
