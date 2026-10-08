/* Host regression against rel17's original literal six-step encoding. */
#include <assert.h>
#include <stdint.h>
#include "hw_six_step.h"
#include "hw_motor_am13e_commutation.h"

int main(void)
{
    static const uint16_t original[6] = {
        0x175U, 0xd9U, 0x1abU, 0x72U, 0x1deU, 0xacU
    };
    /* Original AM13E bring-up forward-sector table, prior to unification. */
    static const uint8_t expected_positive[6] = {4U, 1U, 1U, 2U, 2U, 4U};
    static const uint8_t expected_negative[6] = {2U, 2U, 4U, 4U, 1U, 1U};
    for (unsigned step = 1; step <= 6; ++step) {
        hw_six_step_t phase = hw_six_step_decode(step, 0U);
        am13e_commutation_plan_t plan;
        assert(phase.positive == expected_positive[step - 1U]);
        assert(phase.negative == expected_negative[step - 1U]);
        assert(am13e_commutation_plan(phase.positive, phase.negative, true, &plan));
        assert(plan.positive_mask == phase.positive);
        assert(plan.negative_mask == phase.negative);
        assert(!am13e_commutation_plan(phase.positive, phase.positive, true, &plan));
    }
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        for (unsigned step = 1; step <= 6; ++step) {
            unsigned x = original[step - 1U];
            unsigned m = x >> 3;
            hw_six_step_t actual = hw_six_step_decode(step, reverse);
            assert(actual.positive == (x & m));
            assert(actual.negative == ((~x) & m));
            assert(actual.comparator == ((m >> 3) ^ (reverse << 2)));
            assert((actual.positive & actual.negative) == 0U);
        }
    }
    return 0;
}
