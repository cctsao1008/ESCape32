/* Rel17-owned BEMF state -> hardware scheduling intent.
 * No separate policy, clock assumption, peripheral access or motor output.
 */
#pragma once
#include <stdbool.h>
#include "hw_bemf_rel17_state.h"

typedef struct {
    bool cancel_delay;
    bool arm_delay;
    int delay_ticks;
} hw_bemf_rel17_bridge_action_t;

static inline hw_bemf_rel17_bridge_action_t hw_bemf_rel17_bridge_event(
    hw_bemf_rel17_state_t *state, bool timeout, bool capture_enabled,
    int capture_ticks, int timer_xres, int timing)
{
    hw_bemf_rel17_bridge_action_t action = {false, false, 0};
    if (!state) return action;
    const hw_bemf_rel17_event_t event = hw_bemf_rel17_process(
        state, timeout, capture_enabled, capture_ticks, timer_xres, timing);
    if (event.action == HW_BEMF_REL17_TIMEOUT) {
        action.cancel_delay = true;
    } else if (event.action == HW_BEMF_REL17_CAPTURE) {
        action.arm_delay = true;
        action.delay_ticks = event.delay;
    }
    return action;
}
