/* Offline-only NONMAIN sector image test: no silicon writes.
 * Verify reversibility and ALL non-policy BCR/BSL bytes preservation.
 */
#include "wrp_nonmain.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t original[AM13E_BOOT_NONMAIN_CFG_BYTES];
static uint8_t out[AM13E_BOOT_NONMAIN_CFG_BYTES];
static void le32(uint8_t *p,uint32_t v){
    for(unsigned i=0;i<4U;++i)p[i]=(uint8_t)(v>>(i*8U));
}
static uint32_t rd32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|
           ((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}
static void untouched(void){
    for(unsigned i=0;i<AM13E_BOOT_NONMAIN_CFG_BYTES;++i){
        if((i>=AM13E_BOOT_NONMAIN_MAIN_A_OFFSET&&
            i<AM13E_BOOT_NONMAIN_MAIN_A_OFFSET+4U)||
           (i>=AM13E_BOOT_NONMAIN_MAIN_B_OFFSET&&
            i<AM13E_BOOT_NONMAIN_MAIN_B_OFFSET+4U)||
           (i>=AM13E_BOOT_NONMAIN_BANK1_MAIN_A_OFFSET&&
            i<AM13E_BOOT_NONMAIN_BANK1_MAIN_A_OFFSET+4U)||
           (i>=AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET&&
            i<AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET+4U))continue;
        assert(out[i]==original[i]);
    }
}
int main(void){
    for(unsigned i=0;i<sizeof original;++i) original[i]=(uint8_t)(i*7U+13U);
    le32(original+AM13E_BOOT_NONMAIN_WEP_CONFIG_OFFSET,UINT32_C(0x1234aabb));
    /* Reserve high and low bits in both B words: they must survive
     * Off/Boot/Full modes byte-for-byte, not be erased to zero.
     */
    le32(original+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET,0xcaad0007U);
    le32(original+AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET,0x559a0005U);
    bool dirty=false;
    for(unsigned mode=0U;mode<3U;++mode){
        assert(boot_am13e_nonmain_wrp_prepare(original,sizeof original,
                  (AM13E_NonmainWrpMode)mode,out,sizeof out,&dirty));
        assert(dirty);
        assert(rd32(out+AM13E_BOOT_NONMAIN_MAIN_A_OFFSET)==
               (mode==0U?0U:mode==1U?0xffffU:UINT32_MAX));
        const uint32_t mask=AM13E_BOOT_NONMAIN_MAIN_B_VALID_MASK;
        const uint32_t wanted=mode==2U?mask:0U;
        assert(rd32(out+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET)==
            ((rd32(original+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET)&~mask)|wanted));
        assert(rd32(out+AM13E_BOOT_NONMAIN_BANK1_MAIN_A_OFFSET)==
               (mode==2U?UINT32_MAX:0U));
        assert(rd32(out+AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET)==
            ((rd32(original+AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET)&~mask)|wanted));
        untouched();
        uint8_t previous[AM13E_BOOT_NONMAIN_CFG_BYTES];
        memcpy(previous,original,sizeof previous);
        memcpy(original,out,sizeof out);
        assert(boot_am13e_nonmain_wrp_prepare(original,sizeof original,
                  (AM13E_NonmainWrpMode)mode,out,sizeof out,&dirty));
        assert(!dirty);
        memcpy(original,previous,sizeof original);
    }
    assert(!boot_am13e_nonmain_wrp_prepare(original,2047U,
                  AM13E_WRP_BOOT,out,sizeof out,&dirty));
    assert(!boot_am13e_nonmain_wrp_prepare(original,sizeof original,
                  (AM13E_NonmainWrpMode)3U,out,sizeof out,&dirty));
    assert(!boot_am13e_nonmain_wrp_prepare(original,sizeof original,
                  AM13E_WRP_BOOT,original,sizeof original,&dirty));
    original[AM13E_BOOT_NONMAIN_WEP_CONFIG_OFFSET]=0U;
    assert(!boot_am13e_nonmain_wrp_prepare(original,sizeof original,
                  AM13E_WRP_BOOT,out,sizeof out,&dirty));
    puts("PASS NONMAIN Off/Boot/Full BOTH banks, reserved B bits, 2KiB BCR preserve, no-op/locked reject");
    return 0;
}
