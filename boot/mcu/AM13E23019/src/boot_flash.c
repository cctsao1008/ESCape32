/*
 * AM13E23019 boot Flash backend.
 *
 * The TI high-level DL_Flash_* APIs are used for the inactive bank. The SDK
 * explicitly excludes active-bank use from those APIs, so Bank0 uses a
 * separate transaction path. During each actual Flash command, TI's
 * DL_FlashCTL_executeCommand() executes from .TI.ramfunc. Interrupts are masked
 * around the Bank0 transaction so no ISR can fetch instructions from Bank0
 * while the bank is busy.
 */

#include "boot_flash.h"

#include <stddef.h>

#include "dl_flash.h"
#include "flash_layout.h"

_Static_assert(ESCAPE32_FLASH_SECTOR_SIZE == DL_FLASH_SECTOR_SIZE,
    "Flash sector size must match the AM13E23019 SDK");
_Static_assert(ESCAPE32_FLASH_BANK1_BASE == NVMNW_BANK1_MAIN_ADDRESS,
    "Bank1 base must match the AM13E23019 SDK");
_Static_assert(
    (ESCAPE32_FLASH_BANK0_END - ESCAPE32_FLASH_BANK0_BASE) ==
        DL_FLASH_BANK_SIZE,
    "Bank0 size must match the AM13E23019 SDK");
_Static_assert(
    (ESCAPE32_FLASH_BANK1_END - ESCAPE32_FLASH_BANK1_BASE) ==
        DL_FLASH_BANK_SIZE,
    "Bank1 size must match the AM13E23019 SDK");

static bool boot_flash_range_in_bank(
    uint32_t address, uint32_t size, uint32_t bank_base, uint32_t bank_end)
{
    if ((size == 0U) || (address < bank_base) || (address >= bank_end)) {
        return false;
    }

    return size <= (bank_end - address);
}

bool boot_flash_app_range_valid(uint32_t address, uint32_t size)
{
    if ((size == 0U) ||
        (address < ESCAPE32_APP_BASE) ||
        (address >= ESCAPE32_APP_END)) {
        return false;
    }

    return size <= (ESCAPE32_APP_END - address);
}

static boot_flash_status_t boot_flash_classify_write(
    uint32_t address, uint32_t size, bool *bank0)
{
    if (!boot_flash_app_range_valid(address, size)) {
        return BOOT_FLASH_ERR_RANGE;
    }

    if (boot_flash_range_in_bank(
            address,
            size,
            ESCAPE32_FLASH_BANK0_BASE,
            ESCAPE32_FLASH_BANK0_END)) {
        *bank0 = true;
        return BOOT_FLASH_OK;
    }

    if (boot_flash_range_in_bank(
            address,
            size,
            ESCAPE32_FLASH_BANK1_BASE,
            ESCAPE32_FLASH_BANK1_END)) {
        *bank0 = false;
        return BOOT_FLASH_OK;
    }

    return BOOT_FLASH_ERR_CROSS_BANK;
}

/*
 * Clear Status is also a Flash command (CMDTYPE=5). AM13E TRM 13.3.1
 * requires the CMDEXEC/write-and-wait sequence to execute from SRAM or a
 * different bank. Do not use the Flash-resident DriverLib inline helper here:
 * the compiler may out-of-line it into Bank0 Flash.
 *
 * Keep the TI DriverLib register sequence unchanged; isolate the entire
 * command, including its CMDINPROGRESS poll, in RAM_C.
 */
RAMFUNC static void boot_flash_bank0_clear_status(void)
{
    NVMNW->GEN.CMDTYPE = DL_FLASHCTL_COMMAND_TYPE_CLEAR_STATUS;
    __DSB();
    __ISB();

    NVMNW->GEN.CMDEXEC = NVMNW_CMDEXEC_VAL_EXECUTE;
    __DSB();
    __ISB();

    while ((NVMNW->GEN.STATCMD & NVMNW_STATCMD_CMDINPROGRESS_MASK) ==
           NVMNW_STATCMD_CMDINPROGRESS_STATINPROGRESS) {
    }
}

/*
 * Bank0 helpers intentionally live in .TI.ramfunc.
 *
 * The DriverLib configuration helpers called below execute before a Flash
 * command becomes active. The command itself is launched and polled by TI's
 * RAM-resident DL_FlashCTL_executeCommand(), and control returns only after the
 * Flash controller reports command completion.
 *
 * This is a software/static porting implementation. Active-bank behavior still
 * requires Phase-4 validation on the target board.
 */
RAMFUNC static boot_flash_status_t boot_flash_bank0_erase_sector(
    uint32_t address)
{
    boot_flash_status_t result = BOOT_FLASH_ERR_DRIVER;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    if (DL_FlashCTL_acquireFlashSemaphore() != DL_FLASH_SUCCESS) {
        goto out;
    }

    boot_flash_bank0_clear_status();
    DL_FlashCTL_unprotectSector(
        NVMNW, address, DL_FLASHCTL_REGION_SELECT_MAIN);

    if (DL_FlashCTL_eraseMemory(
            NVMNW, address, DL_FLASHCTL_COMMAND_SIZE_SECTOR) !=
        DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        goto release;
    }

    for (uint32_t current = address;
         current < (address + ESCAPE32_FLASH_SECTOR_SIZE);
         current += 16U) {
        if (DL_FlashCTL_blankVerify(
                NVMNW, current, DL_FLASHCTL_REGION_SELECT_MAIN) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            goto release;
        }
    }

    result = BOOT_FLASH_OK;

release:
    if (DL_FlashCTL_releaseFlashSemaphore() != DL_FLASH_SUCCESS) {
        result = BOOT_FLASH_ERR_DRIVER;
    }

out:
    __set_PRIMASK(primask);
    return result;
}

RAMFUNC static boot_flash_status_t boot_flash_bank0_program(
    uint32_t address, const uint8_t *data, uint32_t size)
{
    boot_flash_status_t result = BOOT_FLASH_ERR_DRIVER;
    uint32_t primask = __get_PRIMASK();
    uint32_t remaining = size;
    uint32_t offset = 0U;
    uint32_t padded[2];

    __disable_irq();

    if (DL_FlashCTL_acquireFlashSemaphore() != DL_FLASH_SUCCESS) {
        goto out;
    }

    while (remaining >= 16U) {
        boot_flash_bank0_clear_status();
        DL_FlashCTL_unprotectSector(
            NVMNW, address, DL_FLASHCTL_REGION_SELECT_MAIN);

        if (DL_FlashCTL_programMemory128WithECCGenerated(
                NVMNW,
                address,
                (const uint32_t *)(const void *)&data[offset]) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            goto release;
        }

        if (DL_FlashCTL_readVerify128WithECCGenerated(
                NVMNW,
                address,
                (const uint32_t *)(const void *)&data[offset]) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            result = BOOT_FLASH_ERR_VERIFY;
            goto release;
        }

        address += 16U;
        offset += 16U;
        remaining -= 16U;
    }

    if (remaining >= 8U) {
        boot_flash_bank0_clear_status();
        DL_FlashCTL_unprotectSector(
            NVMNW, address, DL_FLASHCTL_REGION_SELECT_MAIN);

        if (DL_FlashCTL_programMemory64WithECCGenerated(
                NVMNW,
                address,
                (const uint32_t *)(const void *)&data[offset]) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            goto release;
        }

        if (DL_FlashCTL_readVerify64WithECCGenerated(
                NVMNW,
                address,
                (const uint32_t *)(const void *)&data[offset]) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            result = BOOT_FLASH_ERR_VERIFY;
            goto release;
        }

        address += 8U;
        offset += 8U;
        remaining -= 8U;
    }

    if (remaining > 0U) {
        uint8_t *padded_bytes = (uint8_t *)(void *)padded;

        for (uint32_t i = 0U; i < 8U; ++i) {
            padded_bytes[i] = 0xFFU;
        }
        for (uint32_t i = 0U; i < remaining; ++i) {
            padded_bytes[i] = data[offset + i];
        }

        boot_flash_bank0_clear_status();
        DL_FlashCTL_unprotectSector(
            NVMNW, address, DL_FLASHCTL_REGION_SELECT_MAIN);

        if (DL_FlashCTL_programMemory64WithECCGenerated(
                NVMNW, address, padded) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            goto release;
        }

        if (DL_FlashCTL_readVerify64WithECCGenerated(
                NVMNW, address, padded) !=
            DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            result = BOOT_FLASH_ERR_VERIFY;
            goto release;
        }
    }

    result = BOOT_FLASH_OK;

release:
    if (DL_FlashCTL_releaseFlashSemaphore() != DL_FLASH_SUCCESS) {
        result = BOOT_FLASH_ERR_DRIVER;
    }

out:
    __set_PRIMASK(primask);
    return result;
}

boot_flash_status_t boot_flash_erase_sector(uint32_t address)
{
    bool bank0;

    if ((address % ESCAPE32_FLASH_SECTOR_SIZE) != 0U) {
        return BOOT_FLASH_ERR_ALIGNMENT;
    }

    boot_flash_status_t status =
        boot_flash_classify_write(address, ESCAPE32_FLASH_SECTOR_SIZE, &bank0);
    if (status != BOOT_FLASH_OK) {
        return status;
    }

    if (bank0) {
        return boot_flash_bank0_erase_sector(address);
    }

    return (DL_Flash_eraseSector(address) == DL_FLASH_SUCCESS)
        ? BOOT_FLASH_OK
        : BOOT_FLASH_ERR_DRIVER;
}

boot_flash_status_t boot_flash_program(
    uint32_t address, const uint8_t *data, uint32_t size)
{
    bool bank0;

    if (data == NULL) {
        return BOOT_FLASH_ERR_RANGE;
    }

    /*
     * SDK 26.01.00.03 implementation requires 128-bit address alignment for
     * automatic ECC generation, even though one API comment mentions 8-byte
     * alignment. Follow the implementation requirement.
     */
    if ((address & 0x0FU) != 0U) {
        return BOOT_FLASH_ERR_ALIGNMENT;
    }

    boot_flash_status_t status =
        boot_flash_classify_write(address, size, &bank0);
    if (status != BOOT_FLASH_OK) {
        return status;
    }

    if (bank0) {
        return boot_flash_bank0_program(address, data, size);
    }

    return (DL_Flash_program(address, (uint8_t *)(uintptr_t)data, size) ==
            DL_FLASH_SUCCESS)
        ? BOOT_FLASH_OK
        : BOOT_FLASH_ERR_DRIVER;
}

boot_flash_status_t boot_flash_verify(
    uint32_t address, const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || !boot_flash_app_range_valid(address, size)) {
        return BOOT_FLASH_ERR_RANGE;
    }

    const volatile uint8_t *flash =
        (const volatile uint8_t *)(uintptr_t)address;

    for (uint32_t i = 0U; i < size; ++i) {
        if (flash[i] != data[i]) {
            return BOOT_FLASH_ERR_VERIFY;
        }
    }

    return BOOT_FLASH_OK;
}
