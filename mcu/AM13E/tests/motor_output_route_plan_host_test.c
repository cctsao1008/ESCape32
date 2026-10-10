/* Board-neutral pad-mask/PinCM conflict checks only.
 * No physical GPIO, gate, or waveform simulated.
 */
#include "motor_output_route_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    const uint32_t p[6]={1U<<8U,1U<<11U,1U<<9U,1U<<30U,1U<<10U,1U<<31U};
    const uint32_t m[6]={8U,11U,9U,30U,10U,31U};
    const uint32_t mask=p[0]|p[1]|p[2]|p[3]|p[4]|p[5];
    assert(am13e_motor_pad_route_plan_valid(p,m,mask));
    assert(!am13e_motor_pad_route_plan_valid(p,m,mask & ~p[5]));
    assert(!am13e_motor_pad_route_plan_valid(p,m,0U));
    assert(!am13e_motor_pad_route_plan_valid(NULL,m,mask));
    assert(!am13e_motor_pad_route_plan_valid(p,NULL,mask));
    for (unsigned i=0U;i<6U;++i) {
        uint32_t q[6],pins[6];
        for (unsigned j=0U;j<6U;++j) {q[j]=m[j];pins[j]=p[j];}
        q[i]=107U;
        assert(!am13e_motor_pad_route_plan_valid(p,q,mask));
        pins[i]=0U;
        assert(!am13e_motor_pad_route_plan_valid(pins,m,mask));
        pins[i]=p[(i+1U)%6U];
        assert(!am13e_motor_pad_route_plan_valid(pins,m,mask));
        q[i]=m[(i+1U)%6U];
        assert(!am13e_motor_pad_route_plan_valid(p,q,mask));
    }
    puts("PASS: AM13E 6-pad mask, PINCM uniqueness and invalid-route negatives");
    return 0;
}
