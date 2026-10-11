/* WRP bitmap policy based solely on the original Rel17 three modes
 * and TI NONMAIN BCR offsets. Static bit=1 means protected.
 * Original Boot: first 16 x 1KiB Bank0 MAIN protection units per
 * TI TRM section 13.4.1; physical resolution must be qualified.
 * Full MUST protect 512KiB across BOTH MAIN banks. Preserve every
 * reserved B-field bit; the defined mask is bits [15:4].
 * BCR/BSL/security/password/lifecycle data remain untouched.
 */
#include "wrp_nonmain.h"
#include <stddef.h>
#include <string.h>
static uint32_t get32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|
           ((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}
static void put32(uint8_t *p,uint32_t v){
    for(unsigned i=0U;i<4U;++i)p[i]=(uint8_t)(v>>(i*8U));
}
bool boot_am13e_nonmain_wrp_prepare(const uint8_t *original,
    unsigned n,AM13E_NonmainWrpMode mode,uint8_t *planned,
    unsigned capacity,bool *needs_write)
{
    if(!original||!planned||!needs_write||n!=AM13E_BOOT_NONMAIN_CFG_BYTES||
       capacity<n||((uintptr_t)original==(uintptr_t)planned)||
       (mode!=AM13E_WRP_OFF&&mode!=AM13E_WRP_BOOT&&mode!=AM13E_WRP_FULL))
        return false;
    /* The ROM must be able to reprogram its own BCR/BSL config.
     * WEP_DISABLE=0xaabb per TI TRM; don't try to bypass a
     * configured permanent NONMAIN protection lock.
     */
    if((get32(original+AM13E_BOOT_NONMAIN_WEP_CONFIG_OFFSET)&0xffffU)!=
       AM13E_BOOT_NONMAIN_WEP_DISABLE)
        return false;
    const uint32_t bank0_a=mode==AM13E_WRP_FULL?UINT32_MAX:
          mode==AM13E_WRP_BOOT?UINT32_C(0x0000ffff):UINT32_C(0);
    const uint32_t bank1_a=mode==AM13E_WRP_FULL?UINT32_MAX:UINT32_C(0);
    const uint32_t valid=AM13E_BOOT_NONMAIN_MAIN_B_VALID_MASK;
    const uint32_t b_bits=mode==AM13E_WRP_FULL?valid:UINT32_C(0);
    /* The NONMAIN B-field reserves [3:0] and [31:16] on the
     * 512KiB two-bank device. Never alter those bits on a BCR sector
     * erase/reprogram, even when selecting original WRP Off/Full.
     */
    memcpy(planned,original,n);
    put32(planned+AM13E_BOOT_NONMAIN_MAIN_A_OFFSET,bank0_a);
    put32(planned+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET,
        (get32(original+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET)&~valid)|b_bits);
    put32(planned+AM13E_BOOT_NONMAIN_BANK1_MAIN_A_OFFSET,bank1_a);
    put32(planned+AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET,
        (get32(original+AM13E_BOOT_NONMAIN_BANK1_MAIN_B_OFFSET)&~valid)|b_bits);
    /* No in-place erase/program, no BSL/lifecycle mutation.
     * A no-op bitmap must not wear the NONMAIN sector.
     */
    *needs_write=memcmp(original,planned,n)!=0;
    return true;
}
