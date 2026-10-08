#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "hw_motor_am13e_commutation.h"

static void test_steps(void)
{
    /* Match ESCape32 rel17 nextstep() sequence: C/B, A/B, A/C,
       B/C, B/A, C/A. Masks A=1, B=2, C=4. */
    const unsigned pos[] = {4, 1, 1, 2, 2, 4};
    const unsigned neg[] = {2, 2, 4, 4, 1, 1};
    for (unsigned step = 0; step < 6; step++) {
        for (unsigned damp = 0; damp < 2; damp++) {
            am13e_commutation_plan_t p;
            assert(am13e_commutation_plan(pos[step], neg[step],
                                           damp != 0, &p));
            assert(p.positive_mask == pos[step]);
            assert(p.negative_mask == neg[step]);
            assert(p.damp == (damp != 0));
            unsigned counts[3] = {0};
            for (unsigned i = 0; i < 3; i++) {
                assert((unsigned)p.phase[i] < 3);
                counts[p.phase[i]]++;
                assert(p.phase[i] == ((pos[step] & (1U << i))
                    ? AM13E_PHASE_POSITIVE_PWM
                    : ((neg[step] & (1U << i))
                       ? AM13E_PHASE_NEGATIVE_ON : AM13E_PHASE_FLOAT)));
            }
            assert(counts[0] == 1 && counts[1] == 1 && counts[2] == 1);
        }
    }
    puts("[PASS] all six FW1 steps, damp on/off and phase ownership");
}
static void test_coast_and_rejection(void)
{
    am13e_commutation_plan_t p;
    assert(am13e_commutation_plan(0, 0, false, &p));
    for (unsigned i = 0; i < 3; i++) assert(p.phase[i] == AM13E_PHASE_FLOAT);
    assert(!am13e_commutation_plan(1, 1, true, &p));
    assert(!am13e_commutation_plan(3, 4, false, &p));
    assert(!am13e_commutation_plan(1, 6, false, &p));
    assert(!am13e_commutation_plan(8, 2, false, &p));
    assert(!am13e_commutation_plan(1, 2, false, 0));
    puts("[PASS] zero-throttle coast and invalid phase mask rejection");
}
int main(void) {
    test_steps();
    test_coast_and_rejection();
    puts("[PASS] E62 FW1 commutation policy host regression");
    return 0;
}
