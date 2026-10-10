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

/* Signature resides in ECC16 at APP+0x400 (start of restore block 1).
 * APP+0 vectors are programmed during restore block 0; the marker
 * remains erased until CRC and EVERY code byte are verified.
 * Power loss keeps the image unbootable until a new update completes.
 */
static uint8_t pending_signature[16];
/* 1KiB logical CMD_WRITE block / 2KiB physical erase sector.
 * SRAM_S, 16-byte aligned for pinned TI SDK DL_Flash_program.
 * APP metadata block0/1 retains its distinct signature-last path.
 */
static uint8_t sector_merge[AM13E_FLASH_ERASE_SECTOR]
    __attribute__((aligned(16)));
static bool pending_signature_valid;

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
static unsigned last_block = AM13E_FLASH_APP_BLOCKS;
static unsigned last_length;

/* Test-only simulation of a reset: volatile session state is lost while
 * Flash contents remain intact. Never compiled into an AM13E firmware build.
 */
#ifdef AM13E_FLASH_TEST
void boot_am13e_test_reset_update_state(void) {
    for (unsigned i = 0; i < sizeof pending_signature; ++i)
        pending_signature[i] = 0U;
    pending_signature_valid = false;
    update_phase = UPDATE_IDLE;
    next_block = 2U;
    last_block = AM13E_FLASH_APP_BLOCKS;
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
            if (update_phase == UPDATE_RESTORE_1 && len == 1024)
                return same_flash_block(addr, src, 1024U) ? 1 : 0;
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
        pending_signature_valid = false;
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
            last_block = AM13E_FLASH_APP_BLOCKS;
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

    /* Block 0 now CONTAINS M33 vectors at 0x6000, not a signature.
     * Restore the entire vector table as ordinary Flash data. Its
     * validity cannot be published until block 1's APP+0x400 ECC16
     * marker is written last and CRC verification has succeeded.
     */
    if (addr == first) {
        if (len != 1024)
            return 0;
        if (boot_am13e_flash_execute((uint32_t)addr,
                                     (uint8_t *)(uintptr_t)src,
                                     (uint32_t)len,false,true) !=
            DL_FLASH_SUCCESS)
            return 0;
        if (!same_flash_block(addr,src,(unsigned)len)) return 0;
        pending_signature_valid=false;
        update_phase=UPDATE_RESTORE_1;
        return 1;
    }
    if (addr == first + 1024U && len != 1024)
        return 0;

    /* Block 1 contains the signature at its first ECC16. Hold it
     * in SRAM and program the rest of the block first. The signature
     * must remain all-FF if the process crashes at any prior step.
     */
    if (addr == first + AM13E_IMAGE_SIGNATURE_OFFSET) {
        for (unsigned i=0U;i<16U;++i)
            pending_signature[i]=(uint8_t)src[i];
        pending_signature_valid=true;
        if (boot_am13e_flash_execute((uint32_t)(addr+16U),
                                     (uint8_t *)(uintptr_t)(src+16),
                                     1024U-16U,false,true)!=
            DL_FLASH_SUCCESS) return 0;
        if (!same_flash_block(addr+16U,src+16,1024U-16U))
            return 0;
        const volatile uint8_t *marker=(const volatile uint8_t *)addr;
        for (unsigned i=0U;i<16U;++i)
            if (marker[i]!=UINT8_C(0xff)) return 0;

        const uint32_t received_length=
            (uint32_t)last_block*1024U + (uint32_t)last_length;
        uint32_t image_length=0U;
        if(boot_am13e_image_check(first,end,pending_signature,16U,
                                   &image_length)!=AM13E_IMAGE_VALID ||
           image_length!=received_length) return 0;
        if (boot_am13e_flash_execute((uint32_t)addr,pending_signature,
                                     16U,false,true)!=DL_FLASH_SUCCESS)
            return 0;
        for(unsigned i=0U;i<16U;++i)
            if(marker[i]!=pending_signature[i]) return 0;
        pending_signature_valid=false;
        update_phase=UPDATE_COMPLETE;
        return 1;
    }

    /* Preserve the unaffected 1KiB half on EVERY APP data write.
     * Full 2KiB sector snapshot happens BEFORE any Flash erase/program.
     * Flash DriverLib and controller sequence execute from SRAM_C.
     */
    const uintptr_t sector_first=addr&
                         ~((uintptr_t)DL_FLASH_SECTOR_SIZE-1U);
    const unsigned offset=(unsigned)(addr-sector_first);
    if(sector_first<first+AM13E_IMAGE_METADATA_SECTOR ||
       sector_first>end || end-sector_first<DL_FLASH_SECTOR_SIZE ||
       offset+1024U>DL_FLASH_SECTOR_SIZE)
        return 0;
    const volatile uint8_t *old=(const volatile uint8_t *)sector_first;
    for(unsigned i=0;i<DL_FLASH_SECTOR_SIZE;++i)
        sector_merge[i]=old[i];
    for(unsigned i=0;i<1024U;++i)
        sector_merge[offset+i]=i<(unsigned)len?
                               (uint8_t)src[i]:UINT8_C(0xff);
    if(boot_am13e_flash_execute((uint32_t)sector_first,
                                 sector_merge,DL_FLASH_SECTOR_SIZE,
                                 true,true)!=DL_FLASH_SUCCESS)
        return 0;
    const volatile uint8_t *verify=(const volatile uint8_t *)sector_first;
    for(unsigned i=0;i<DL_FLASH_SECTOR_SIZE;++i)
        if(verify[i]!=sector_merge[i])return 0;

    /* All further blocks are >=2 (application code/data). We never
     * permit the usual program branch to publish an image marker.
     */
    {
        last_block = block;
        last_length = (unsigned)len;
        ++next_block;
    }
    return 1;
}
