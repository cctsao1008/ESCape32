/* Host regression for exact STM32G431 Rel17 TIM2 timeout.
 * This is arithmetic/porting verification, NOT on-board validation.
 */
#include <assert.h>
#include <stdint.h>
#include "motor_bemf.h"

int main(void)
{
    enum { LEGACY_CLK_MHZ=168, LEGACY_IFTIM_XRES=2 };
    const uint32_t psc=(LEGACY_CLK_MHZ>>(LEGACY_IFTIM_XRES+1))-1U;
    const uint32_t arr=(UINT32_C(1)<<(LEGACY_IFTIM_XRES+16))-1U;
    const uint64_t timer_hz=UINT64_C(168000000)/(psc+1U);
    const uint64_t overflow_us=((uint64_t)(arr+1U)*UINT64_C(1000000))/timer_hz;
    assert(psc==20U && arr==UINT32_C(262143));
    assert(timer_hz==UINT64_C(8000000));
    assert(overflow_us==UINT64_C(32768));
    assert(overflow_us==(uint64_t)AM13E_BEMF_REL17_TIMEOUT_US);
    /* All currently accepted ECAP clock rates fit 32-bit TSCTR. */
    assert((uint64_t)AM13E_BEMF_REL17_TIMEOUT_US*200U <= UINT32_MAX);
    return 0;
}
