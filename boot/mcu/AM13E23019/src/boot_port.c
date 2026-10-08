/*
 * AM13E23019 boot platform adapter.
 */

#include "boot_port.h"

#include <stddef.h>

#include "boot_flash.h"
#include "flash_layout.h"
#include "soc.h"

bool boot_port_app_valid(void)
{
    const volatile uint32_t *vector =
        (const volatile uint32_t *)(uintptr_t)ESCAPE32_APP_BASE;
    uint32_t initial_sp = vector[0];
    uint32_t reset_pc   = vector[1];
    uint32_t reset_addr = reset_pc & ~1UL;

    if ((initial_sp < ESCAPE32_RAM_S_BASE) ||
        (initial_sp > ESCAPE32_RAM_S_END) ||
        ((initial_sp & 0x7UL) != 0UL)) {
        return false;
    }

    if ((reset_pc & 0x1UL) == 0UL) {
        return false;
    }

    if ((reset_addr < ESCAPE32_APP_BASE) ||
        (reset_addr >= ESCAPE32_APP_END)) {
        return false;
    }

    return true;
}

__attribute__((noreturn))
void boot_port_jump_to_app(void)
{
    const volatile uint32_t *vector =
        (const volatile uint32_t *)(uintptr_t)ESCAPE32_APP_BASE;
    uint32_t initial_sp = vector[0];
    uint32_t reset_pc   = vector[1];

    /*
     * Boot uses polling only and does not enable NVIC sources. Keep PRIMASK in
     * its reset state so the application does not inherit interrupts disabled.
     */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    SCB->VTOR = ESCAPE32_APP_BASE;
    __DSB();
    __ISB();

    __set_CONTROL(0U);
    __ISB();

    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(initial_sp), "r"(reset_pc)
        : "memory");

    __builtin_unreachable();
}

const char *boot_port_map_read(uint32_t offset, uint32_t length)
{
    if ((length == 0U) ||
        (offset >= ESCAPE32_APP_SIZE) ||
        (length > (ESCAPE32_APP_SIZE - offset))) {
        return NULL;
    }

    return (const char *)(uintptr_t)(ESCAPE32_APP_BASE + offset);
}

bool boot_port_write_block(
    uint32_t offset, const char *data, uint32_t length)
{
    if ((data == NULL) ||
        (length == 0U) ||
        (offset >= ESCAPE32_APP_SIZE) ||
        (length > (ESCAPE32_APP_SIZE - offset))) {
        return false;
    }

    uint32_t address = ESCAPE32_APP_BASE + offset;
    uint32_t end = address + length - 1U;

    /*
     * Preserve the upstream ESCape32 sequential-write erase behavior:
     * erase a sector when the current write begins exactly on it or first
     * crosses into it. A following 1-KiB block in the second half of the same
     * 2-KiB AM13E sector must not erase the first half again.
     */
    uint32_t erase =
        (address + ESCAPE32_FLASH_SECTOR_SIZE - 1U) &
        ~(ESCAPE32_FLASH_SECTOR_SIZE - 1U);
    uint32_t last =
        end & ~(ESCAPE32_FLASH_SECTOR_SIZE - 1U);

    while (erase <= last) {
        if (boot_flash_erase_sector(erase) != BOOT_FLASH_OK) {
            return false;
        }

        erase += ESCAPE32_FLASH_SECTOR_SIZE;
    }

    if (boot_flash_program(
            address,
            (const uint8_t *)(const void *)data,
            length) != BOOT_FLASH_OK) {
        return false;
    }

    return boot_flash_verify(
        address,
        (const uint8_t *)(const void *)data,
        length) == BOOT_FLASH_OK;
}
