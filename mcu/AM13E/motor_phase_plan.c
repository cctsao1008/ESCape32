#include "motor_phase_plan.h"
#include <stddef.h>

/* EXACT lower three bits of Rel17 src/main.c seq[]={0x175,0xd9,
 * 0x1ab,0x72,0x1de,0xac}. The 3-bit COMP code is (x >> 6)
 * and is not the floating phase's one-hot mask.
 */
static const uint8_t positive[6] = {4U, 1U, 1U, 2U, 2U, 4U};
static const uint8_t negative[6] = {2U, 2U, 4U, 4U, 1U, 1U};
static const uint8_t comp_code[6] = {5U, 3U, 6U, 1U, 7U, 2U};

int am13e_motor_plan_sixstep(int positive_mask, int negative_mask,
                            int comparator_code, int damp, int reverse,
                            AM13E_SixstepPlan *out)
{
    if (out == NULL || positive_mask < 0 || negative_mask < 0 ||
        (positive_mask | negative_mask) > 7 ||
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
        if (positive_mask == (int)positive[step] &&
            negative_mask == (int)negative[step] &&
            comparator_code ==
                (int)(comp_code[step] ^ (plan.reverse ? 4U : 0U))) {
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
