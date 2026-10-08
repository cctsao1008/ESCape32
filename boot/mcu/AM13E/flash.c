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
#include "image_integrity.h"

#ifdef AM13E_FLASH_TEST
extern uintptr_t boot_am13e_test_first;
extern uintptr_t boot_am13e_test_end;
#define BOOT_APP_FIRST boot_am13e_test_first
#define BOOT_APP_END boot_am13e_test_end
#else
extern char __app_flash_start__[];
extern char __boot_storage_end__[];
#define BOOT_APP_FIRST ((uintptr_t)__app_flash_start__)
#define BOOT_APP_END ((uintptr_t)__boot_storage_end__)
#endif

/* RAM-only staging of the application signature program unit.
 * Never publish the valid signature before both final metadata blocks
 * have been verified. Reset loses the staged signature safely.
 */
static uint8_t pending_header[16];
static bool pending_header_valid;

/* One session follows the unchanged WiFi-Link ordering:
 * invalidate 0, invalidate 1, sequential blocks 2..N, restore 0, restore 1.
 * A lost ACK may repeat the most recently accepted block with identical data.
 * State is intentionally volatile: a reset requires a fresh invalidation.
 */
enum update_phase {
    UPDATE_IDLE,
    UPDATE_INVALIDATED_0,
    UPDATE_PROGRAM,
    UPDATE_RESTORE_1,
    UPDATE_COMPLETE
};
static enum update_phase update_phase;
static unsigned next_block = 2U;
static unsigned last_block = 256U;
static unsigned last_length;

/* Test-only simulation of a reset: volatile session state is lost while
 * Flash contents remain intact. Never compiled into an AM13E firmware build.
 */
#ifdef AM13E_FLASH_TEST
void boot_am13e_test_reset_update_state(void) {
    for (unsigned i = 0; i < sizeof pending_header; ++i)
        pending_header[i] = 0U;
    pending_header_valid = false;
    update_phase = UPDATE_IDLE;
    next_block = 2U;
    last_block = 256U;
    last_length = 0U;
}
#endif

static bool same_flash_block(uintptr_t addr, const char *src, unsigned len) {
    const volatile uint8_t *flash = (const volatile uint8_t *)addr;
    for (unsigned i = 0; i < len; ++i)
        if (flash[i] != (uint8_t)src[i])
            return false;
    return true;
}

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
    uintptr_t end = BOOT_APP_END;
    uintptr_t first = BOOT_APP_FIRST;
    if (!src || len <= 0 || (addr & 15U) != 0U || addr < first ||
        addr >= end || (unsigned)len > end - addr ||
        (unsigned)len > 1024U)
        return 0;

    const unsigned block = (unsigned)((addr - first) / 1024U);
    if (len != 8) {
        if (update_phase == UPDATE_PROGRAM && block >= 2U) {
            if (block == last_block && (unsigned)len == last_length)
                return same_flash_block(addr, src, (unsigned)len) ? 1 : 0;
            if (block != next_block)
                return 0;
        } else if (block == 0U) {
            /* A lost final ACK may resend an already committed block 0.
             * Compare only; never erase or reprogram a valid image.
             */
            if (update_phase == UPDATE_COMPLETE && len == 1024)
                return same_flash_block(addr, src, 1024U) ? 1 : 0;
            if (update_phase == UPDATE_RESTORE_1 && len == 1024 &&
                pending_header_valid) {
                for (unsigned i = 0; i < sizeof pending_header; ++i)
                    if (pending_header[i] != (uint8_t)src[i])
                        return 0;
                return same_flash_block(addr + sizeof pending_header,
                                        src + sizeof pending_header,
                                        1024U - sizeof pending_header) ? 1 : 0;
            }
            if (update_phase != UPDATE_PROGRAM || next_block <= 2U ||
                len != 1024)
                return 0;
        } else if (block == 1U) {
            if (update_phase == UPDATE_COMPLETE && len == 1024)
                return same_flash_block(addr, src, 1024U) ? 1 : 0;
            if (update_phase != UPDATE_RESTORE_1 || len != 1024)
                return 0;
        } else {
            return 0;
        }
    }

    /* WiFi-Link signature invalidation: two 8-byte all-FF writes to
     * application blocks 0 and 1. The first erases the full 2 KiB
     * metadata sector; the second only confirms that it is blank.
     * No generic 8-byte program is attempted (ECC requires 16 bytes).
     */
    if (len == 8) {
        /* Block 0 invalidation is an explicit request to begin another
         * update. Replaying it before block 1 is safe and idempotent.
         * Once programming began, this deliberately starts a new session.
         */
        if (block == 1U &&
            update_phase != UPDATE_INVALIDATED_0 &&
            !(update_phase == UPDATE_PROGRAM && next_block == 2U))
            return 0;
        /* Check the payload before changing the session state. */
        if (addr != first && addr != first + 1024U)
            return 0;
        for (unsigned i = 0; i < 8U; ++i)
            if ((uint8_t)src[i] != UINT8_C(0xff))
                return 0;
        pending_header_valid = false;
        if (addr == first &&
            boot_am13e_flash_execute((uint32_t)first, 0, 0, true, false) != DL_FLASH_SUCCESS)
            return 0;
        const volatile uint8_t *check = (const volatile uint8_t *)first;
        for (unsigned i = 0; i < DL_FLASH_SECTOR_SIZE; ++i)
            if (check[i] != UINT8_C(0xff))
                return 0;
        if (block == 0U) {
            update_phase = UPDATE_INVALIDATED_0;
            next_block = 2U;
            last_block = 256U;
            last_length = 0U;
        } else {
            update_phase = UPDATE_PROGRAM;
        }
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
        update_phase = UPDATE_RESTORE_1;
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
        /* The host protocol has no end-of-image command. Therefore we
         * require the authenticated image length to match exactly the
         * sequential data span acknowledged in this transaction.
         * Flash bytes for the metadata sector have now been programmed
         * and read back, but the application signature is still erased.
         */
        uint32_t image_length = 0U;
        const uint32_t received_length =
            (uint32_t)last_block * 1024U + (uint32_t)last_length;
        if (boot_am13e_image_check(first, end, pending_header,
                                   sizeof pending_header, &image_length) !=
                AM13E_IMAGE_VALID ||
            image_length != received_length)
            return 0;

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
        update_phase = UPDATE_COMPLETE;
    } else {
        last_block = block;
        last_length = (unsigned)len;
        ++next_block;
    }
    return 1;
}
