/* Test the production SRAM-only AM13E Boot stage with native GCC.
 * Completeness/authenticity/Flash commit are intentionally NOT claimed.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "update_staging.h"
#define CHECK(x) do { if(!(x)){ \
    fprintf(stderr,"FAIL staging:%d %s\\n",__LINE__,#x);exit(1);}}while(0)
static void put32(uint8_t *p,uint32_t v){
    for(unsigned i=0;i<4U;i++)p[i]=(uint8_t)(v>>(8U*i));
}
static void full_test(void){
    boot_am13e_stage_begin();
    CHECK(!boot_am13e_stage_complete());
    CHECK(!boot_am13e_stage_accept(1U,1024U));
    for(unsigned i=0;i<16U;i++){
        uint8_t *b=boot_am13e_stage_next(i);
        CHECK(b!=NULL);
        memset(b,0x87,1024U);
        if(i==0U){put32(b,UINT32_C(0x20018000));put32(b+4,0x101U);}
        CHECK(boot_am13e_stage_accept(i,1024U));
        CHECK(boot_am13e_stage_length()==(i+1U)*1024U);
    }
    CHECK(boot_am13e_stage_complete());
    CHECK(boot_am13e_stage_vector_plausible());
    CHECK(boot_am13e_stage_next(16U)==NULL);
    CHECK(!boot_am13e_stage_accept(16U,4U));
    boot_am13e_stage_abort();
    CHECK(boot_am13e_stage_length()==0U);
    CHECK(!boot_am13e_stage_complete());
    CHECK(boot_am13e_stage_next(0U)==NULL);
    puts("PASS staging 16KiB bounded and 17th frame rejected");
}
static void short_test(void){
    boot_am13e_stage_begin();
    uint8_t *first=boot_am13e_stage_next(0U);
    CHECK(first!=NULL);
    for(unsigned i=0;i<1024U;i++)CHECK(first[i]==0U);
    memset(first,0x55,1024U);
    CHECK(!boot_am13e_stage_accept(0U,0U));
    CHECK(!boot_am13e_stage_accept(0U,2U));
    CHECK(!boot_am13e_stage_accept(0U,1025U));
    CHECK(boot_am13e_stage_accept(0U,1024U));
    CHECK(!boot_am13e_stage_accept(0U,1024U));
    uint8_t *second=boot_am13e_stage_next(1U);
    CHECK(second!=NULL);
    memset(second,0x33,4U);
    CHECK(boot_am13e_stage_accept(1U,4U));
    CHECK(boot_am13e_stage_complete());
    CHECK(boot_am13e_stage_length()==1028U);
    CHECK(boot_am13e_stage_next(2U)==NULL);
    boot_am13e_stage_abort();
    boot_am13e_stage_begin();
    CHECK(boot_am13e_stage_next(0U)!=NULL);
    CHECK(boot_am13e_stage_accept(0U,1020U));
    CHECK(boot_am13e_stage_length()==1020U);
    CHECK(boot_am13e_stage_complete());
    boot_am13e_stage_abort();
    puts("PASS staging 4B/1020B short tails, bad sizes, duplicate, abort and clear");
}
static void vector_test(void){
    const struct {uint32_t sp,pc;int ok;} vectors[]={
        {0x20000008U,0x00000009U,1},
        {0x20018000U,0x00000009U,1},
        {0x20000004U,0x00000009U,0},
        {0x20018008U,0x00000009U,0},
        {0x00c18000U,0x00000009U,0},
        {0x20000008U,0x00000008U,0},
        {0x20000008U,0x00000401U,0},
        {0x20000008U,0x00004001U,0},
        {0xffffffffU,0xffffffffU,0}
    };
    for(unsigned i=0;i<sizeof vectors/sizeof vectors[0];i++){
        boot_am13e_stage_begin();
        uint8_t *b=boot_am13e_stage_next(0U);
        CHECK(b!=NULL);
        memset(b,0xff,16U);
        put32(b,vectors[i].sp);
        put32(b+4,vectors[i].pc);
        CHECK(!boot_am13e_stage_vector_plausible());
        CHECK(boot_am13e_stage_accept(0U,16U));
        CHECK(boot_am13e_stage_vector_plausible()==(bool)vectors[i].ok);
        boot_am13e_stage_abort();
    }
    puts("PASS staging native M33 Boot-vector plausibility checks");
}
int main(void){
    full_test();short_test();vector_test();
    puts("PASS CMD_UPDATE staged SRAM helper (NO BANK0 FLASH COMMIT)");
    return 0;
}
