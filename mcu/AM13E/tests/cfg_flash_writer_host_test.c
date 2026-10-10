#include "cfg_flash_writer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint8_t flash[8192]; /* 0x4000..0x5fff: FW1 + FW2 sentinel */
    unsigned erase_count, program_count, read_count;
    unsigned fail_erase_at,fail_program_at,fail_read_at;
    int corrupt_readback;
} Mock;
static int erase(uint32_t addr, void *ptr)
{
    Mock *m=ptr; ++m->erase_count;
    if(m->fail_erase_at==m->erase_count) return 0;
    if(addr!=0x4000U && addr!=0x4800U) return 0;
    memset(m->flash+addr-0x4000U,0xff,2048U);return 1;
}
static int program(uint32_t addr,const uint32_t data[4],void *ptr)
{
    Mock *m=ptr;++m->program_count;
    if(m->fail_program_at==m->program_count) return 0;
    if(addr<0x4000U || addr>0x4ff0U || (addr&15U)) return 0;
    const uint8_t *p=(const uint8_t*)data;
    for(unsigned i=0;i<16U;i++)
        if ((m->flash[addr-0x4000U+i]&p[i])!=p[i]) return 0;
    for(unsigned i=0;i<16U;i++)
        m->flash[addr-0x4000U+i]&=p[i];
    return 1;
}
static int readback(uint32_t addr,uint8_t out[16],void *ptr)
{
    Mock *m=ptr;++m->read_count;
    if(m->fail_read_at==m->read_count) return 0;
    if(addr<0x4000U || addr>0x4ff0U || (addr&15U)) return 0;
    memcpy(out,m->flash+addr-0x4000U,16U);
    if(m->corrupt_readback) out[0]^=1U;
    return 1;
}
static void prepare(Mock *m)
{
    memset(m,0,sizeof(*m));
    memset(m->flash,0x55,sizeof(m->flash));
}
int main(void)
{
    static uint8_t data[4096];
    static Mock m;
    for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*23U+11U);
    const uint32_t lengths[]={1U,7U,15U,16U,17U,128U,2047U,2048U,2049U,4079U,4080U,4081U,4096U};
    for(unsigned n=0;n<sizeof lengths/sizeof lengths[0];++n){
        const uint32_t len=lengths[n];
        AM13E_CfgFlashPlan plan;
        assert(am13e_cfg_flash_plan(0x4000U,0x20001000U,len,&plan));
        prepare(&m);
        AM13E_CfgFlashOps ops={erase,program,readback,&m};
        assert(am13e_cfg_flash_execute(&plan,data,&ops));
        assert(m.erase_count==(len<=2048U?1U:2U));
        assert(m.program_count==plan.padded_program_bytes/16U);
        assert(m.read_count==m.program_count);
        assert(!memcmp(m.flash,data,len));
        for(uint32_t i=len;i<plan.padded_program_bytes;i++)
            assert(m.flash[i]==0xffU);
        /* The FW2 0x5000..0x5fff sentinel is never modified. */
        for(uint32_t i=4096U;i<8192U;i++)
            assert(m.flash[i]==0x55U);
        /* Out-of-payload sector remains untouched. */
        if(len<=2048U)
            for(uint32_t i=2048U;i<4096U;i++)assert(m.flash[i]==0x55U);
    }
    AM13E_CfgFlashPlan plan;
    assert(am13e_cfg_flash_plan(0x4000U,0x20001000U,2049U,&plan));
    AM13E_CfgFlashOps ops={erase,program,readback,&m};
    prepare(&m);m.fail_erase_at=2U;
    assert(!am13e_cfg_flash_execute(&plan,data,&ops));
    prepare(&m);m.fail_program_at=2U;
    assert(!am13e_cfg_flash_execute(&plan,data,&ops));
    prepare(&m);m.fail_read_at=2U;
    assert(!am13e_cfg_flash_execute(&plan,data,&ops));
    prepare(&m);m.corrupt_readback=1;
    assert(!am13e_cfg_flash_execute(&plan,data,&ops));
    prepare(&m);
    AM13E_CfgFlashPlan bad=plan;bad.destination=0x5000U;
    assert(!am13e_cfg_flash_execute(&bad,data,&ops));
    /* An oversized forged plan must NOT erase more sectors than the
     * original settings payload requires, even if its sector count and
     * ECC alignment look internally consistent.
     */
    bad=plan;bad.byte_count=1U;bad.padded_program_bytes=4096U;
    bad.sector_count=2U;
    prepare(&m);
    assert(!am13e_cfg_flash_execute(&bad,data,&ops));
    assert(m.erase_count==0U && m.program_count==0U);
    bad=plan;bad.padded_program_bytes=4096U;bad.sector_count=2U;
    prepare(&m);
    assert(!am13e_cfg_flash_execute(&bad,data,&ops));
    assert(m.erase_count==0U && m.program_count==0U);
    bad=plan;bad.sector_count=1U;
    assert(!am13e_cfg_flash_execute(&bad,data,&ops));
    bad=plan;bad.padded_program_bytes=4097U;
    assert(!am13e_cfg_flash_execute(&bad,data,&ops));
    assert(!am13e_cfg_flash_execute(&plan,data,NULL));
    assert(!am13e_cfg_flash_execute(&plan,NULL,&ops));
    assert(!am13e_cfg_flash_execute(NULL,data,&ops));
    puts("AM13E FW1 Flash commit engine 13 valid lengths, rollback-not-claimed, failure injection PASS");
    return 0;
}
