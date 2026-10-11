#include "motor_phase_plan.h"
#include <stddef.h>

/* EXACT Rel17 src/main.c seq[]={0x175,0xd9,0x1ab,0x72,0x1de,
 * 0xac}, m=x>>3, positive=x&m, negative=~x&m. The source passes
 * these FULL 6-bit masks, not just (p&7)/(n&7), into sixstep_write().
 * COMP code is m>>3 and is NOT the floating phase one-hot bit.
 */
#include "../../../src/commutation.h"

int am13e_motor_plan_sixstep(int positive_mask, int negative_mask,
                            int comparator_code, int damp, int reverse,
                            AM13E_SixstepPlan *out)
{
    if (out == NULL || positive_mask < 0 || negative_mask < 0 ||
        (positive_mask | negative_mask) > 63 ||
        (positive_mask & negative_mask) != 0 ||
        comparator_code < 0 || comparator_code > 7) {
        return 0;
    }
    AM13E_SixstepPlan plan = {0};
    plan.comparator_code = (uint8_t)comparator_code;
    plan.damp = (damp != 0);
    plan.reverse = (reverse != 0);

    if (positive_mask == 0 && negative_mask == 0) {
        /* Rel17 throt_ztc: no positive or negative bridge drive. */
        *out = plan;
        return 1;
    }
    int found = 0;
    for (unsigned step = 0U; step < 6U; ++step) {
        const unsigned x=rel17_commutation_sequence[step],m=x>>3U;
        if (positive_mask == (int)(x&m) &&
            negative_mask == (int)(~x&m) &&
            comparator_code == (int)((m>>3U) ^ (plan.reverse ? 4U : 0U))) {
            found = 1;
            break;
        }
    }
    if (!found) return 0;

    for (unsigned bit = 0U; bit < 3U; ++bit) {
        const unsigned mask = 1U << bit;
        if ((unsigned)positive_mask & mask)
            plan.phase[bit] = AM13E_PHASE_POSITIVE_PWM;
        else if ((unsigned)negative_mask & mask)
            plan.phase[bit] = AM13E_PHASE_NEGATIVE_SINK;
        else
            plan.phase[bit] = AM13E_PHASE_FLOATING;
    }
    *out = plan;
    return 1;
}
