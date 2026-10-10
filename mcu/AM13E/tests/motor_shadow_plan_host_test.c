#include "motor_shadow_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AM13E_MotorShadowPlan p={.period=6250U,.compare={0U,100U,1250U,2500U,5000U,6250U}};
    assert(am13e_motor_shadow_validate(&p));
    for(unsigned i=0U;i<6U;++i){
        const uint16_t save=p.compare[i];
        p.compare[i]=(uint16_t)(p.period+1U);
        assert(!am13e_motor_shadow_validate(&p));
        p.compare[i]=save;
    }
    p.period=1U;assert(!am13e_motor_shadow_validate(&p));
    p.period=0U;assert(!am13e_motor_shadow_validate(&p));
    p.period=65535U;
    for(unsigned i=0U;i<6U;++i)p.compare[i]=65535U;
    assert(am13e_motor_shadow_validate(&p));
    p.period=2U;
    for(unsigned i=0U;i<6U;++i)p.compare[i]=0U;
    assert(am13e_motor_shadow_validate(&p));
    assert(!am13e_motor_shadow_validate(NULL));
    puts("E1-AR MCPWM0 six independent Compare Shadow bounds PASS");
    return 0;
}
