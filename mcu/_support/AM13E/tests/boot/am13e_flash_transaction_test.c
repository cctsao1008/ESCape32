/* Host compiles production AM13E flash.c: 1KiB Rel17 CMD_WRITE,
 * 2KiB physical RMW, arbitrary valid block order and true image length.
 * Original Boot does NOT guarantee post-interruption image completeness.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "dl_flash.h"
#include "app_validity.h"

uintptr_t boot_am13e_test_first;
uintptr_t boot_am13e_test_end;
extern int boot_am13e_flash_write(char *dst,const char *src,int len);
extern void boot_am13e_test_reset_update_state(void);

#define MAP_ADDRESS UINT32_C(0x10000000)
#define MAP_LENGTH UINT32_C(0x00080000)
#define APP_OFFSET UINT32_C(0x00006000)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",                  __FILE__,__LINE__,#c);exit(1);}}while(0)
static unsigned erase_count,prog_count,fail_erase,fail_program;
static void reset_app(void){
    memset((void *)boot_am13e_test_first,0xff,
           (size_t)(boot_am13e_test_end-boot_am13e_test_first));
    boot_am13e_test_reset_update_state();
}
uint32_t DL_Flash_eraseSector(uint32_t addr){
    if(fail_erase){--fail_erase;return DL_FLASH_ERROR;}
    if((addr&(DL_FLASH_SECTOR_SIZE-1U)) ||
       (uintptr_t)addr<boot_am13e_test_first ||
       (uintptr_t)addr>boot_am13e_test_end-DL_FLASH_SECTOR_SIZE)
        return DL_FLASH_ERROR;
    ++erase_count;
    memset((void *)(uintptr_t)addr,0xff,DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}
uint32_t DL_Flash_program(uint32_t addr,uint8_t *src,uint32_t bytes){
    if(fail_program){--fail_program;return DL_FLASH_ERROR;}
    if(!src || !bytes || (addr&15U) || (bytes&15U) ||
       (uintptr_t)addr<boot_am13e_test_first ||
       (uintptr_t)addr>boot_am13e_test_end-bytes)
        return DL_FLASH_ERROR;
    uint8_t *p=(uint8_t *)(uintptr_t)addr;
    for(uint32_t i=0;i<bytes;++i)
        if((p[i]&src[i])!=src[i])return DL_FLASH_ERROR;
    ++prog_count;
    for(uint32_t i=0;i<bytes;++i)p[i]&=src[i];
    return DL_FLASH_SUCCESS;
}
static int write_block(unsigned n,const uint8_t *b,unsigned len){
    return boot_am13e_flash_write(
      (char *)(uintptr_t)(boot_am13e_test_first+1024U*n),
      (const char *)b,(int)len);
}
static void base_tests(void){
    uint8_t block2[1024],block3[1024],blank[8];
    memset(block2,0x35,sizeof block2);
    memset(block3,0x4e,sizeof block3);
    memset(blank,0xff,sizeof blank);
    reset_app();
    /* Strict original CMD_WRITE is APP-only; 1KB index never refers
     * to the Boot or 4KiB Config / Reserved partitions.
     */
    CHECK(!boot_am13e_flash_write((char *)(uintptr_t)MAP_ADDRESS,
                                   (const char *)block2,1024));
    CHECK(!write_block(488U,block2,1024));
    CHECK(!write_block(2U,block2,3U));
    CHECK(!write_block(2U,block2,1025U));
    CHECK(!write_block(2U,NULL,16U));

    /* Arbitrary source order is now legal; no v1.6 all-image
     * metadata state machine is required by CMD_WRITE.
     */
    CHECK(write_block(3U,block3,1024));
    unsigned before=erase_count;
    CHECK(write_block(2U,block2,1024));
    CHECK(erase_count==before+1U);
    CHECK(memcmp((void *)(boot_am13e_test_first+2U*1024U),
                 block2,1024U)==0);
    CHECK(memcmp((void *)(boot_am13e_test_first+3U*1024U),
                 block3,1024U)==0);
    before=erase_count;
    CHECK(write_block(2U,block2,1024));
    CHECK(erase_count==before); /* retried identical frame is a no-op */
    puts("PASS 1KiB/2KiB RMW preserves neighbor, arbitrary order, retry");

    uint8_t small[20];
    memset(small,0xa7,sizeof small);
    CHECK(write_block(4U,small,sizeof small));
    CHECK(memcmp((void *)(boot_am13e_test_first+4096U),
                 small,sizeof small)==0);
    puts("PASS real linked firmware tail may be <1KiB");

    reset_app();
    fail_erase=1U;
    CHECK(!write_block(2U,block2,1024));
    CHECK(write_block(2U,block2,1024));
    fail_program=1U;
    CHECK(!write_block(3U,block3,1024));
    CHECK(write_block(3U,block3,1024));
    puts("PASS injected erase/program failure allows retry");

    /* Old host's 8-byte FF invalidation remains an ordinary
     * complementary/CRC-framed flash write, no hidden signature.
     */
    uint8_t vec[1024];
    memset(vec,0x11,sizeof vec);
    CHECK(write_block(0U,vec,1024));
    CHECK(write_block(1U,block2,1024));
    CHECK(write_block(0U,blank,8U));
    CHECK(memcmp((void *)boot_am13e_test_first,blank,8U)==0);
    CHECK(memcmp((void *)(boot_am13e_test_first+1024U),
                 block2,1024U)==0);
    CHECK(write_block(1U,blank,8U));
    puts("PASS original 8-byte FF update invalidation, no signature");
}
static uint8_t *read_image(const char *filename,size_t *size){
    FILE *f=fopen(filename,"rb");
    if(!f){perror(filename);exit(2);}
    CHECK(!fseek(f,0,SEEK_END));
    const long n=ftell(f);
    CHECK(n>=8 && (unsigned long)n<=AM13E_FLASH_APP_BYTES &&
          !(n&3L));
    CHECK(!fseek(f,0,SEEK_SET));
    uint8_t *data=malloc((size_t)n);
    CHECK(data && fread(data,1U,(size_t)n,f)==(size_t)n);
    CHECK(!fclose(f));
    *size=(size_t)n;return data;
}
static void transfer_image(const char *filename){
    size_t size=0U;
    uint8_t *binary=read_image(filename,&size);
    reset_app();
    /* Preserve all three disjoint regions during APP-only writes. */
    memset((void *)(uintptr_t)MAP_ADDRESS,0x42,0x4000U);
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x4000U),0xff,0x1000U);
    *(uint16_t *)(uintptr_t)(MAP_ADDRESS+0x4000U)=AM13E_BOOT_CFG_ID;
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x5000U),0x44,0x1000U);
    /* No signed image header or CRC is needed; real binary size is
     * the number of CMD_WRITE data bytes, <=488KiB.
     */
    for(size_t off=0;off<size;off+=1024U) {
        size_t len=size-off;
        if(len>1024U)len=1024U;
        CHECK(write_block((unsigned)(off/1024U),binary+off,(unsigned)len));
    }
    CHECK(memcmp((void *)boot_am13e_test_first,binary,size)==0);
    uint32_t sp=0,pc=0;
    CHECK(boot_am13e_app_validity(
          (void *)(uintptr_t)(MAP_ADDRESS+0x4000U),
          (void *)boot_am13e_test_first,&sp,&pc));
    /* A later program data byte can be corrupted without changing
     * Rel17 Cfg+vector validity. This is a KNOWN limitation, not CRC.
     */
    if(size>64U){
        *((uint8_t *)boot_am13e_test_first+size-1U)^=1U;
        CHECK(boot_am13e_app_validity(
              (void *)(uintptr_t)(MAP_ADDRESS+0x4000U),
              (void *)boot_am13e_test_first,&sp,&pc));
    }
    for(unsigned i=0;i<0x4000U;++i)
        CHECK(*((uint8_t *)(uintptr_t)(MAP_ADDRESS+i))==0x42U);
    for(unsigned i=0;i<0x1000U;++i)
        CHECK(*((uint8_t *)(uintptr_t)(MAP_ADDRESS+0x5000U+i))==0x44U);
    puts("PASS variable-length ARM-linked flat BIN across full APP bounds");
    puts("PASS Cfg.id/APP vector boot after update; Boot+Cfg+Reserved preserved");
    puts("LIMIT: no full-image CRC or interruption-completeness guarantee");
    free(binary);
}
int main(int argc,char **argv){
    void *map=mmap((void *)(uintptr_t)MAP_ADDRESS,MAP_LENGTH,
                   PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|
                   MAP_FIXED_NOREPLACE,-1,0);
    CHECK(map!=MAP_FAILED);
    boot_am13e_test_first=MAP_ADDRESS+APP_OFFSET;
    boot_am13e_test_end=MAP_ADDRESS+MAP_LENGTH;
    memset(map,0xff,MAP_LENGTH);
    if(argc==3 && strcmp(argv[1],"--flash-image")==0){
        transfer_image(argv[2]);return 0;
    }
    if(argc!=1){fprintf(stderr,"Usage: %s [--flash-image file]\n",argv[0]);return 2;}
    base_tests();
    return 0;
}
