/* Native AM13E23019 original Boot write() for APP — Rel17 v1.4.
 * The original CMD_WRITE accepts a 1KiB logical block (or short tail)
 * and returns ACK/NAK; it DOES NOT enforce a whole-image transfer
 * sequence, flash-resident CRC, or signature-last commit.
 *
 * A 1KiB write uses a 2KiB physical sector read/modify/erase/program
 * with SRAM staging to preserve the companion half and allow retries.
 * Same-bank erase/program operations execute from SRAM_C; on-target
 * wear, recovery and power-failure tests remain mandatory.
 */
#include "common.h"
#include <dl_flash.h>
#include <soc.h>
#include <stdint.h>
#include "../../../mcu/AM13E/flash_partition.h"

#ifdef AM13E_FLASH_TEST
extern uintptr_t boot_am13e_test_first;
extern uintptr_t boot_am13e_test_end;
#define APP_FIRST boot_am13e_test_first
#define APP_END boot_am13e_test_end
/* No volatile signature/session policy in the original Rel17 protocol. */
void boot_am13e_test_reset_update_state(void) {}
#else
extern char __app_flash_start__[];
extern char __boot_storage_end__[];
#define APP_FIRST ((uintptr_t)__app_flash_start__)
#define APP_END ((uintptr_t)__boot_storage_end__)
#endif

static uint8_t sector_image[AM13E_FLASH_ERASE_SECTOR]
    __attribute__((aligned(16)));

/* Do not infer static WRP from the temporary CMDWEPROT registers,
 * which DriverLib intentionally unprotects. The SDK exposes latched
 * GSC attributes, but the documented page granularities conflict.
 * Until silicon-qualified, conservatively reject a bank with ANY
 * static MAIN protection. This is deliberately stricter than the
 * desired Boot-only mode; SETWRP qualification remains blocked.
 */
#ifdef AM13E_FLASH_TEST
bool boot_am13e_test_protected;
#endif
static bool bank_writable(uintptr_t address)
{
#ifdef AM13E_FLASH_TEST
    (void)address;
    return !boot_am13e_test_protected;
#else
    return address < UINT32_C(0x40000) ?
        !(GSC->FPC_WEPROTA0 || (GSC->FPC_WEPROTB0 & UINT32_C(0xfff0))) :
        !(GSC->FPC_WEPROTA1 || (GSC->FPC_WEPROTB1 & UINT32_C(0xfff0)));
#endif
}

/* On AM13E this function and the entire Flash Controller busy path are
 * placed in SRAM_C; linker and MAP checks verify DriverLib placement.
 * Do NOT access or execute code from the affected Flash bank while busy.
 */
__attribute__((noinline,section(".TI.ramfunc")))
static uint32_t flash_commit(uint32_t address,uint8_t *data)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();__DSB();__ISB();
    uint32_t status=DL_Flash_eraseSector(address);
    if(status==DL_FLASH_SUCCESS)
        status=DL_Flash_program(address,data,AM13E_FLASH_ERASE_SECTOR);
    __DSB();__ISB();
    __set_PRIMASK(mask);
    return status;
}

/* Host CTest must avoid collision with libc's POSIX write() symbol.
 * Only the symbol name is substituted; the C function body is exactly
 * the same as the original Rel17 write() entry linked into ARM Boot.
 */
#ifdef AM13E_FLASH_TEST
#define AM13E_WRITE_ENTRY boot_am13e_flash_write
#else
#define AM13E_WRITE_ENTRY write
#endif
int AM13E_WRITE_ENTRY(char *dst,const char *source,int length)
{
    const uintptr_t app=APP_FIRST,addr=(uintptr_t)dst;
    if(app<AM13E_FLASH_APP_BASE-AM13E_FLASH_FW1_PARAM_BASE)return 0;
    const uintptr_t cfg=app-(AM13E_FLASH_APP_BASE-AM13E_FLASH_FW1_PARAM_BASE);
    const bool config=addr>=cfg && addr<cfg+AM13E_FLASH_CFG_BLOCKS*1024U;
    const uintptr_t first=config?cfg:app;
    const uintptr_t end=config?cfg+AM13E_FLASH_CFG_BLOCKS*1024U:APP_END;
    if(!source || length<=0 || length>(int)AM13E_FLASH_LOGICAL_BLOCK ||
       (length&3) || first>=end
#ifndef AM13E_FLASH_TEST
       || app!=AM13E_FLASH_APP_BASE || APP_END!=AM13E_FLASH_APP_END
#endif
       || addr<first || addr>=end || (addr&15U) ||
       ((addr-first)%AM13E_FLASH_LOGICAL_BLOCK)!=0U ||
       (unsigned)length>end-addr || !bank_writable(addr))
        return 0;
    const uintptr_t sector=addr&
        ~((uintptr_t)AM13E_FLASH_ERASE_SECTOR-1U);
    const unsigned offset=(unsigned)(addr-sector);
    if(sector<first || sector>end ||
       end-sector<AM13E_FLASH_ERASE_SECTOR ||
       offset+(unsigned)length>AM13E_FLASH_ERASE_SECTOR)
        return 0;

    const volatile uint8_t *old=(const volatile uint8_t *)sector;
    int identical=1;
    for(int i=0;i<length;++i)
        if(old[offset+(unsigned)i]!=(uint8_t)source[i]){
            identical=0;break;
        }
    if(identical)return 1; /* Retry does not consume another erase cycle. */
    for(unsigned i=0;i<AM13E_FLASH_ERASE_SECTOR;++i)
        sector_image[i]=old[i];
    for(int i=0;i<length;++i)
        sector_image[offset+(unsigned)i]=(uint8_t)source[i];
    if(flash_commit((uint32_t)sector,sector_image)!=DL_FLASH_SUCCESS)
        return 0;
    const volatile uint8_t *actual=(const volatile uint8_t *)sector;
    for(unsigned i=0;i<AM13E_FLASH_ERASE_SECTOR;++i)
        if(actual[i]!=sector_image[i])return 0;
    return 1;
}
