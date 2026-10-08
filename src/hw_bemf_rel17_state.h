/* ESCape32 rel17 zero-cross ISR state transitions; no peripheral access. */
#pragma once
#include "hw_bemf_rel17_math.h"

typedef struct {
    int interval;
    int electrical_time;
    int sync;
    int fast;
} hw_bemf_rel17_state_t;

typedef enum {
    HW_BEMF_REL17_IGNORE = 0,
    HW_BEMF_REL17_TIMEOUT,
    HW_BEMF_REL17_CAPTURE
} hw_bemf_rel17_action_t;

typedef struct {
    hw_bemf_rel17_action_t action;
    int delay;
} hw_bemf_rel17_event_t;

static inline hw_bemf_rel17_event_t hw_bemf_rel17_process(
    hw_bemf_rel17_state_t *state, int timeout_pending,
    int capture_enabled, int capture_ticks, int timer_xres, int timing)
{
    hw_bemf_rel17_event_t result = {HW_BEMF_REL17_IGNORE, 0};
    /* rel17 checks timeout first, even if capture is simultaneously ready. */
    if (timeout_pending) {
        state->sync = 0;
        state->fast = 0;
        state->interval = 10000 << timer_xres;
        state->electrical_time = 100000000;
        result.action = HW_BEMF_REL17_TIMEOUT;
        return result;
    }
    if (!capture_enabled || capture_ticks < (state->interval >> 1))
        return result;

    const hw_bemf_rel17_result_t math = hw_bemf_rel17_calculate(
        capture_ticks, state->interval, state->electrical_time, timing);
    state->fast = math.fast;
    state->interval = math.interval;
    if (state->sync < 6) ++state->sync;
    result.action = HW_BEMF_REL17_CAPTURE;
    result.delay = math.delay;
    return result;
}
