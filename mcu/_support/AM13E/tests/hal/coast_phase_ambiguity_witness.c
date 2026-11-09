/* UNIT indistinguishability witness. No vectors in this file are supplied
 * to native QEMU. One-code noise is a hypothesis, not measured hardware. */
#include "coast_phase_plan.h"
#include <assert.h>
#include <stdio.h>
static int replay(void)
{
    static const unsigned patterns[]={5,5,1,1,3,3};
    const uint16_t reference[]={1000,1000,1000};
    am13e_coast_phase_reset();
    for(unsigned n=0;n<6U;++n){
        AM13E_AdcRaw r={0};
        r.sample_count=n+1U;r.acquisition_tick=r.completion_tick=(n+1U)*16U;
        r.phase_a=(uint16_t)(1000U+!!(patterns[n]&1U));
        r.phase_b=(uint16_t)(1000U+!!(patterns[n]&2U));
        r.phase_c=(uint16_t)(1000U+!!(patterns[n]&4U));
        am13e_coast_phase_update(&r,7U);
    }
    return am13e_coast_phase_select(96U,7U,0,reference,0);
}
int main(void)
{
    const int desired_forward=replay(),stationary_one_code_noise=replay();
    assert(desired_forward==4 && stationary_one_code_noise==desired_forward);
    /* Identical digital evidence cannot identify an unobserved hidden
     * trajectory. A full calibrated analog speed/amplitude bound may
     * exclude aliases; this witness does not establish such a bound. */
    const int forward_sector[]={0,0,1,1,2,2};
    const int reverse_alias_sector[]={0,0,-5,-5,-10,-10};
    for(unsigned n=0;n<6U;++n)assert((forward_sector[n]%6+6)%6==(reverse_alias_sector[n]%6+6)%6);
    puts("PASS_NEW UNIT witness: real planner authorizes identical one-code history under forward versus stationary-noise hypotheses");
    puts("INCOMPLETE physical direction qualification: independent noise/offset/amplitude/sampling bounds required; modulo-sector alias is not a measured native trajectory");
    return 0;
}
