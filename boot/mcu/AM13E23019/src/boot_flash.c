/*
 * E62 AM13E23019 boot Flash backend.
 *
 * The TI high-level DL_Flash_* APIs are used only for Bank1 while the boot
 * image executes from Bank0.  The target SDK explicitly documents that those
 * APIs must not be used on the active Flash bank.  Bank0 support therefore
 * needs a separate RAM-resident transaction path.
 */

#include "boot_flash.h"

#include <stddef.h>

#include "dl_flash.h"
#include "e62_flash_layout.h"

_Static_assert(E62_FLASH_SECTOR_SIZE == DL_FLASH_SECTOR_SIZE,
    "E62 sector size must match the AM13E23019 SDK");
_Static_assert(E62_FLASH_BANK1_BASE == NVMNW_BANK1_MAIN_ADDRESS,
    "E62 Bank1 base must match the AM13E23019 SDK");
_Static_assert((E62_FLASH_BANK0_END - E62_FLASH_BANK0_BASE) == DL_FLASH_BANK_SIZE,
    "E62 Bank0 size must match the AM13E23019 SDK");
_Static_assert((E62_FLASH_BANK1_END - E62_FLASH_BANK1_BASE) == DL_FLASH_BANK_SIZE,
    "E62 Bank1 size must match the AM13E23019 SDK");

static bool e62_boot_flash_range_in_bank(
    uint32_t address, uint32_t size, uint32_t bank_base, uint32_t bank_end)
{
    if ((size == 0U) || (address < bank_base) || (address >= bank_end)) {
        return false;
    }

    return size <= (bank_end - address);
}

bool e62_boot_flash_app_range_valid(uint32_t address, uint32_t size)
{
    if ((size == 0U) || (address < E62_APP_BASE) || (address >= E62_APP_END)) {
        return false;
    }

    return size <= (E62_APP_END - address);
}

static e62_boot_flash_status_t e62_boot_flash_classify_write(
    uint32_t address, uint32_t size, bool *bank0)
{
    if (!e62_boot_flash_app_range_valid(address, size)) {
        return E62_BOOT_FLASH_ERR_RANGE;
    }

    if (e62_boot_flash_range_in_bank(
            address, size, E62_FLASH_BANK0_BASE, E62_FLASH_BANK0_END)) {
        *bank0 = true;
        return E62_BOOT_FLASH_OK;
    }

    if (e62_boot_flash_range_in_bank(
            address, size, E62_FLASH_BANK1_BASE, E62_FLASH_BANK1_END)) {
        *bank0 = false;
        return E62_BOOT_FLASH_OK;
    }

    return E62_BOOT_FLASH_ERR_CROSS_BANK;
}

e62_boot_flash_status_t e62_boot_flash_erase_sector(uint32_t address)
{
    bool bank0;

    if ((address % E62_FLASH_SECTOR_SIZE) != 0U) {
        return E62_BOOT_FLASH_ERR_ALIGNMENT;
    }

    e62_boot_flash_status_t status =
        e62_boot_flash_classify_write(address, E62_FLASH_SECTOR_SIZE, &bank0);
    if (status != E62_BOOT_FLASH_OK) {
        return status;
    }

    /*
     * Boot executes from Bank0.  Do not call the normal DL_Flash_* path for
     * active-bank P/E; a complete RAM-resident transaction will be added next.
     */
    if (bank0) {
        return E62_BOOT_FLASH_ERR_ACTIVE_BANK;
    }

    return (DL_Flash_eraseSector(address) == DL_FLASH_SUCCESS)
        ? E62_BOOT_FLASH_OK
        : E62_BOOT_FLASH_ERR_DRIVER;
}

e62_boot_flash_status_t e62_boot_flash_program(
    uint32_t address, const uint8_t *data, uint32_t size)
{
    bool bank0;

    if (data == NULL) {
        return E62_BOOT_FLASH_ERR_RANGE;
    }

    /*
     * SDK 26.01.00.03 implementation requires 128-bit address alignment for
     * automatic ECC generation, even though one API comment mentions 8-byte
     * alignment.  Follow the implementation requirement.
     */
    if ((address & 0x0FU) != 0U) {
        return E62_BOOT_FLASH_ERR_ALIGNMENT;
    }

    e62_boot_flash_status_t status =
        e62_boot_flash_classify_write(address, size, &bank0);
    if (status != E62_BOOT_FLASH_OK) {
        return status;
    }

    if (bank0) {
        return E62_BOOT_FLASH_ERR_ACTIVE_BANK;
    }

    return (DL_Flash_program(address, (uint8_t *)(uintptr_t)data, size) ==
            DL_FLASH_SUCCESS)
        ? E62_BOOT_FLASH_OK
        : E62_BOOT_FLASH_ERR_DRIVER;
}

e62_boot_flash_status_t e62_boot_flash_verify(
    uint32_t address, const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || !e62_boot_flash_app_range_valid(address, size)) {
        return E62_BOOT_FLASH_ERR_RANGE;
    }

    const volatile uint8_t *flash =
        (const volatile uint8_t *)(uintptr_t)address;

    for (uint32_t i = 0U; i < size; ++i) {
        if (flash[i] != data[i]) {
            return E62_BOOT_FLASH_ERR_VERIFY;
        }
    }

    return E62_BOOT_FLASH_OK;
}
