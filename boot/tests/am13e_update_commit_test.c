/* Host tests the exact AM13E Boot-sector commit core, not an image
 * protocol replacement or permission to program a physical Boot Bank0.
 * Run against a mapped Flash mock; production CMD_UPDATE remains NAK.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "dl_flash.h"
#include "update_staging.h"
#include "update_commit.h"

#define TEST_BASE UINT32_C(0x10000000)
#define TOTAL_FLASH UINT32_C(0x00080000)
#define BOOT_LENGTH UINT32_C(0x00004000)
#define CFG_OFFSET UINT32_C(0x00004000)
#define RESERVED_OFFSET UINT32_C(0x00005000)
#define APP_OFFSET UINT32_C(0x00006000)
#define TEST_ASSERT(x) do { if(!(x)){ \
    fprintf(stderr,"FAIL Boot sector commit:%d %s\n",__LINE__,#x); \
    exit(1); }} while(0)

static unsigned erase_attempts,program_attempts;
static unsigned fail_erase_on,fail_program_on,corrupt_on;
static uint8_t expected[BOOT_LENGTH];

static void put_u32(uint8_t *dst,uint32_t value)
{
    for(unsigned i=0;i<4U;++i)dst[i]=(uint8_t)(value>>(i*8U));
}
static uint8_t *flash_at(unsigned offset)
{
    return (uint8_t *)(uintptr_t)(TEST_BASE+offset);
}
static void clear_mock(void)
{
    memset(flash_at(0),0x71,BOOT_LENGTH);
    memset(flash_at(CFG_OFFSET),0x43,0x1000U);
    memset(flash_at(RESERVED_OFFSET),0x44,0x1000U);
    memset(flash_at(APP_OFFSET),0x55,TOTAL_FLASH-APP_OFFSET);
    erase_attempts=program_attempts=0U;
    fail_erase_on=fail_program_on=corrupt_on=0U;
    boot_am13e_stage_abort();
}
static void check_outside_boot(void)
{
    for(unsigned i=CFG_OFFSET;i<RESERVED_OFFSET;++i)
        TEST_ASSERT(flash_at(i)[0]==0x43U);
    for(unsigned i=RESERVED_OFFSET;i<APP_OFFSET;++i)
        TEST_ASSERT(flash_at(i)[0]==0x44U);
    for(unsigned i=APP_OFFSET;i<TOTAL_FLASH;++i)
        TEST_ASSERT(flash_at(i)[0]==0x55U);
}
uint32_t DL_Flash_eraseSector(uint32_t addr)
{
    if((addr&(DL_FLASH_SECTOR_SIZE-1U))!=0U ||
       addr<TEST_BASE || addr>=TEST_BASE+BOOT_LENGTH)
        return DL_FLASH_ERROR;
    ++erase_attempts;
    if(erase_attempts==fail_erase_on)return DL_FLASH_ERROR;
    memset((void *)(uintptr_t)addr,0xff,DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}
uint32_t DL_Flash_program(uint32_t addr,uint8_t *src,uint32_t length)
{
    if(src==NULL || ((uintptr_t)src&15U) || (addr&15U) ||
       length!=DL_FLASH_SECTOR_SIZE || addr<TEST_BASE ||
       addr>TEST_BASE+BOOT_LENGTH-length)
        return DL_FLASH_ERROR;
    ++program_attempts;
    if(program_attempts==fail_program_on)return DL_FLASH_ERROR;
    uint8_t *dst=(uint8_t *)(uintptr_t)addr;
    for(unsigned i=0;i<length;++i){
        if((dst[i]&src[i])!=src[i])return DL_FLASH_ERROR;
        dst[i]&=src[i];
    }
    if(program_attempts==corrupt_on)dst[31U]^=1U;
    return DL_FLASH_SUCCESS;
}
static void make_staged_image(unsigned bytes)
{
    TEST_ASSERT(bytes>=8U && bytes<=BOOT_LENGTH && (bytes&3U)==0U);
    boot_am13e_stage_begin();
    memset(expected,0xff,sizeof expected);
    for(unsigned offset=0;offset<bytes;offset+=1024U){
        const unsigned len=bytes-offset<1024U?bytes-offset:1024U;
        uint8_t *dst=boot_am13e_stage_next(offset/1024U);
        TEST_ASSERT(dst!=NULL);
        for(unsigned j=0;j<len;++j)dst[j]=
            (uint8_t)((offset+j)*37U+0x19U);
        if(offset==0U){
            put_u32(dst,UINT32_C(0x20018000));
            put_u32(dst+4,UINT32_C(0x00000101));
        }
        memcpy(expected+offset,dst,len);
        TEST_ASSERT(boot_am13e_stage_accept(offset/1024U,len));
    }
    TEST_ASSERT(boot_am13e_stage_length()==bytes);
    TEST_ASSERT(boot_am13e_stage_complete());
    TEST_ASSERT(boot_am13e_stage_vector_plausible());
}
static void successful_transfer(unsigned image_bytes)
{
    clear_mock();
    make_staged_image(image_bytes);
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_OK);
    TEST_ASSERT(erase_attempts==8U&&program_attempts==8U);
    TEST_ASSERT(memcmp(flash_at(0),expected,BOOT_LENGTH)==0);
    check_outside_boot();
    boot_am13e_stage_abort();
}
static void invalid_image_tests(void)
{
    clear_mock();
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_BAD_IMAGE);
    TEST_ASSERT(erase_attempts==0U&&program_attempts==0U);
    boot_am13e_stage_begin();
    uint8_t *data=boot_am13e_stage_next(0U);
    TEST_ASSERT(data!=NULL);
    put_u32(data,UINT32_C(0xffffffff));
    put_u32(data+4,UINT32_C(0xffffffff));
    TEST_ASSERT(boot_am13e_stage_accept(0U,8U));
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_BAD_IMAGE);
    TEST_ASSERT(erase_attempts==0U);
    check_outside_boot();
    boot_am13e_stage_abort();
    puts("PASS invalid/incomplete Boot transfer rejected before Flash");
}
static void injected_failure_tests(void)
{
    clear_mock();make_staged_image(1024U*5U+20U);
    fail_erase_on=4U;
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_ERASE_FAILED);
    TEST_ASSERT(erase_attempts==4U&&program_attempts==3U);
    TEST_ASSERT(flash_at(8192U)[0]==0x71U);
    check_outside_boot();

    clear_mock();make_staged_image(BOOT_LENGTH);
    fail_program_on=2U;
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_PROGRAM_FAILED);
    TEST_ASSERT(erase_attempts==2U&&program_attempts==2U);
    TEST_ASSERT(flash_at(4096U)[0]==0x71U);
    check_outside_boot();

    clear_mock();make_staged_image(BOOT_LENGTH);
    corrupt_on=2U;
    TEST_ASSERT(boot_am13e_update_commit_host_run(TEST_BASE)==
                AM13E_BOOT_COMMIT_VERIFY_FAILED);
    TEST_ASSERT(erase_attempts==2U&&program_attempts==2U);
    TEST_ASSERT(flash_at(4096U)[0]==0x71U);
    check_outside_boot();
    boot_am13e_stage_abort();
    puts("PASS sector erase/program/verify failures halt subsequent writes");
}
int main(void)
{
    void *mapped=mmap((void *)(uintptr_t)TEST_BASE,TOTAL_FLASH,
                      PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(mapped==MAP_FAILED){perror("mmap");return 2;}
    invalid_image_tests();
    successful_transfer(1024U*12U+20U);
    successful_transfer(BOOT_LENGTH);
    puts("PASS 16KiB/short Boot image, eight 2KiB sectors, 16-byte alignment");
    injected_failure_tests();
    puts("PASS quarantined SRAM Boot Flash executor Host mock only");
    TEST_ASSERT(munmap(mapped,TOTAL_FLASH)==0);
    return 0;
}
