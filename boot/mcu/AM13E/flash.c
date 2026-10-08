/*
** AM13E application Flash programming using TI DriverLib.
** Flash command execution and interrupt masking are RAM-resident.
** The 1 KiB transport / 2 KiB sector update contract still needs
** hardware qualification and recovery testing.
*/
#include "common.h"
#include <dl_flash.h>
#include <soc.h>
#include <stdint.h>

extern char __app_flash_start__[];
extern char __boot_storage_end__[];

/* RAM-only staging of the application signature program unit.
 * Never publish the valid signature before both final metadata blocks
 * have been verified. Reset loses the staged signature safely.
 */
static uint8_t pending_header[16];
static bool pending_header_valid;

/* Called from Flash; runs wholly in SRAM while its target bank is busy.
 * No application Flash reads or protocol I/O occur in this critical region.
 */
__attribute__((noinline, section(".TI.ramfunc")))
static uint32_t boot_am13e_flash_execute(uint32_t address,
                                         uint8_t *buffer,
                                         uint32_t bytes,
                                         bool erase,
                                         bool program) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();

    uint32_t result = DL_FLASH_SUCCESS;
    if (erase)
        result = DL_Flash_eraseSector(address);
    if (result == DL_FLASH_SUCCESS && program)
        result = DL_Flash_program(address, buffer, bytes);

    __DSB();
    __ISB();
    __set_PRIMASK(primask);
    return result;
}

int boot_am13e_flash_write(char *dst, const char *src, int len) {
    uintptr_t addr = (uintptr_t)dst;
    uintptr_t end = (uintptr_t)__boot_storage_end__;
    uintptr_t first = (uintptr_t)__app_flash_start__;
    if (!src || len <= 0 || (addr & 15U) != 0U || addr < first ||
        addr >= end || (unsigned)len > end - addr ||
        (unsigned)len > 1024U)
        return 0;

    /* WiFi-Link signature invalidation: two 8-byte all-FF writes to
     * application blocks 0 and 1. The first erases the full 2 KiB
     * metadata sector; the second only confirms that it is blank.
     * No generic 8-byte program is attempted (ECC requires 16 bytes).
     */
    if (len == 8) {
        pending_header_valid = false;
        if (addr != first && addr != first + 1024U)
            return 0;
        for (unsigned i = 0; i < 8U; ++i)
            if ((uint8_t)src[i] != UINT8_C(0xff))
                return 0;
        if (addr == first &&
            boot_am13e_flash_execute((uint32_t)first, 0, 0, true, false) != DL_FLASH_SUCCESS)
            return 0;
        const volatile uint8_t *check = (const volatile uint8_t *)first;
        for (unsigned i = 0; i < DL_FLASH_SECTOR_SIZE; ++i)
            if (check[i] != UINT8_C(0xff))
                return 0;
        return 1;
    }
    /* WiFi-Link sends four-byte-aligned final blocks. AM13E Flash
     * programming operates on complete 16-byte units; pad only the
     * unwritten tail with erased (0xff) bytes. Never read beyond src.
     */
    if (((unsigned)len & 3U) != 0U)
        return 0;

    /* The host restores metadata block 0 before metadata block 1.
     * Delay the first 16-byte program unit (which contains 0x32ea)
     * until block 1 has been written and read back successfully.
     * This prevents an interrupted metadata restore from publishing
     * a valid signature prematurely.
     */
    if (addr == first) {
        if (len != 1024)
            return 0;
        pending_header_valid = false;
        for (unsigned i = 0; i < sizeof pending_header; ++i)
            pending_header[i] = (uint8_t)src[i];
        if (boot_am13e_flash_execute((uint32_t)addr + 16U,
                                     (uint8_t *)(uintptr_t)(src + 16),
                                     (uint32_t)len - 16U, false,
                                     true) != DL_FLASH_SUCCESS)
            return 0;
        const volatile uint8_t *verify = (const volatile uint8_t *)addr;
        for (int i = 16; i < len; ++i)
            if (verify[i] != (uint8_t)src[i])
                return 0;
        pending_header_valid = true;
        return 1;
    }
    if (addr == first + 1024U && (len != 1024 || !pending_header_valid))
        return 0;

    /* Only erase on the first 1 KiB block of each 2 KiB sector.
     * The host protocol MUST transmit ordered blocks; an interruption
     * between blocks can leave an incomplete application image.
     */
    const uint32_t aligned_len = (uint32_t)len & ~UINT32_C(15);
    const uint32_t tail_len = (uint32_t)len - aligned_len;
    if (boot_am13e_flash_execute((uint32_t)addr,
                                 (uint8_t *)(uintptr_t)src, aligned_len,
                                 (addr % DL_FLASH_SECTOR_SIZE) == 0U,
                                 aligned_len != 0U) != DL_FLASH_SUCCESS)
        return 0;
    if (tail_len != 0U) {
        uint8_t tail[16];
        for (unsigned i = 0U; i < sizeof tail; ++i)
            tail[i] = UINT8_C(0xff);
        for (unsigned i = 0U; i < tail_len; ++i)
            tail[i] = (uint8_t)src[aligned_len + i];
        if (boot_am13e_flash_execute((uint32_t)(addr + aligned_len),
                                     tail, sizeof tail, false, true) !=
            DL_FLASH_SUCCESS)
            return 0;
    }

    const volatile uint8_t *verify = (const volatile uint8_t *)addr;
    for (int i = 0; i < len; ++i) {
        if (verify[i] != (uint8_t)src[i])
            return 0;
    }

    if (addr == first + 1024U) {
        pending_header_valid = false;
        if (boot_am13e_flash_execute((uint32_t)first,
                                     pending_header,
                                     sizeof pending_header,
                                     false, true) != DL_FLASH_SUCCESS)
            return 0;
        const volatile uint8_t *header = (const volatile uint8_t *)first;
        for (unsigned i = 0; i < sizeof pending_header; ++i)
            if (header[i] != pending_header[i])
                return 0;
    }
    return 1;
}
