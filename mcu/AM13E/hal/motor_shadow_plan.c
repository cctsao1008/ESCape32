#include "motor_shadow_plan.h"
#include <stddef.h>
int am13e_motor_shadow_validate(const AM13E_MotorShadowPlan *plan)
{
    if (plan == NULL || plan->period < 2U) return 0;
    for (unsigned i=0U; i<6U; ++i)
        if (plan->compare[i] > plan->period) return 0;
    return 1;
}
