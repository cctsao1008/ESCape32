/*
 * ESCape32 rel17 BEMF zero-cross calculation, independent of timer hardware.
 * This preserves the integer arithmetic used by iftim_isr().
 */
#pragma once

typedef struct {
    int fast;
    int interval;
    int delay;
} hw_bemf_rel17_result_t;

static inline hw_bemf_rel17_result_t hw_bemf_rel17_calculate(
    int capture, int previous_interval, int revolution_time, int timing)
{
    const int u = previous_interval * 3;
    const int interval = (capture + u) >> 2;
    const int candidate = (interval - (interval * timing >> 5)) >> 1;
    const hw_bemf_rel17_result_t result = {
        .fast = (capture < (u >> 2) || capture > (u >> 1)) &&
            revolution_time < 2000,
        .interval = interval,
        .delay = candidate > 1 ? candidate : 1
    };
    return result;
}
