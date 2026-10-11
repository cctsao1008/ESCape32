#include "cfg_flash_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AM13E_CfgFlashPlan p={0};
    unsigned total=0;
    for(uint32_t n=1;n<=4096U;++n) {
        assert(am13e_cfg_flash_plan(0x4000U,0x20001000U,n,&p));
        assert(p.destination==0x4000U && p.source==0x20001000U);
        assert(p.byte_count==n);
        assert(p.padded_program_bytes>=n && p.padded_program_bytes%16U==0U);
        assert(p.padded_program_bytes<=4096U);
        assert(p.sector_count==(n<=2048U ? 1U : 2U));
        ++total;
    }
    assert(!am13e_cfg_flash_plan(0x4000U,0x20001000U,0U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,0x20001000U,4097U,&p));
    assert(!am13e_cfg_flash_plan(0x5000U,0x20001000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x6000U,0x20001000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x0000U,0x20001000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x47F0U,0x20001000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,0x4000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,0x20018000U,80U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,0x20017FF0U,17U,&p));
    assert(am13e_cfg_flash_plan(0x4000U,0x20017FF0U,16U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,UINTPTR_MAX,16U,&p));
    assert(!am13e_cfg_flash_plan(0x4000U,0x20001000U,80U,NULL));
    printf("AM13E FW1 config Flash partition/ECC plan %u lengths PASS\n",total);
    return 0;
}
