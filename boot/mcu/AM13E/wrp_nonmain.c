/* WRP bitmap policy based solely on the original Rel17 three modes
 * and TI NONMAIN BCR offsets. Static bit=1 means protected.
 * AM13E Boot occupies first 16 x 1KiB Bank0 MAIN protection units.
 * Reserved/BCR/BSL/security/passwords remain untouched.
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
    const uint32_t mask_a=mode==AM13E_WRP_FULL?UINT32_MAX:
          mode==AM13E_WRP_BOOT?UINT32_C(0x0000ffff):UINT32_C(0);
    const uint32_t mask_b=mode==AM13E_WRP_FULL?UINT32_MAX:UINT32_C(0);
    memcpy(planned,original,n);
    put32(planned+AM13E_BOOT_NONMAIN_MAIN_A_OFFSET,mask_a);
    put32(planned+AM13E_BOOT_NONMAIN_MAIN_B_OFFSET,mask_b);
    /* No in-place erase/program, no BSL/lifecycle mutation.
     * A no-op bitmap must not wear the NONMAIN sector.
     */
    *needs_write=memcmp(original,planned,n)!=0;
    return true;
}
