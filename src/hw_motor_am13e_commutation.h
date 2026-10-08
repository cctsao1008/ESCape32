/*
 * E62 FW1 six-step phase policy. Pure C, independent of TI registers.
 * No output is enabled by this module.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum am13e_phase_mode {
    AM13E_PHASE_FLOAT = 0,
    AM13E_PHASE_POSITIVE_PWM = 1,
    AM13E_PHASE_NEGATIVE_ON = 2
};
typedef struct {
    enum am13e_phase_mode phase[3];
    bool damp;
    uint8_t positive_mask;
    uint8_t negative_mask;
} am13e_commutation_plan_t;

/* Normal six-step: exactly one positive, one negative, one floating.
 * Zero-throttle coast: both masks zero. Invalid masks are rejected.
 */
static inline bool am13e_commutation_plan(
    unsigned positive, unsigned negative, bool damp,
    am13e_commutation_plan_t *out)
{
    if (out == 0 || ((positive | negative) & ~7U) != 0U ||
        (positive & negative) != 0U) {
        return false;
    }
    if ((positive | negative) != 0U) {
        if (positive == 0U || (positive & (positive - 1U)) != 0U ||
            negative == 0U || (negative & (negative - 1U)) != 0U) {
            return false;
        }
    }
    out->positive_mask = (uint8_t)positive;
    out->negative_mask = (uint8_t)negative;
    out->damp = damp;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned bit = 1U << i;
        out->phase[i] = (positive & bit)
            ? AM13E_PHASE_POSITIVE_PWM
            : ((negative & bit) ? AM13E_PHASE_NEGATIVE_ON : AM13E_PHASE_FLOAT);
    }
    return true;
}
