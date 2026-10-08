/* Host regression for one-owner rel17 BEMF event dispatch. */
#include <assert.h>
#include <stdio.h>
#include "hw_bemf_rel17_bridge.h"

int main(void)
{
    hw_bemf_rel17_state_t state = {400, 100000, 0, 0};
    hw_bemf_rel17_bridge_action_t a;
    a = hw_bemf_rel17_bridge_event(&state, false, true, 100, 0, 16);
    assert(!a.arm_delay && !a.cancel_delay);
    assert(state.interval == 400 && state.sync == 0);

    a = hw_bemf_rel17_bridge_event(&state, false, true, 400, 0, 16);
    assert(a.arm_delay && !a.cancel_delay);
    assert(a.delay_ticks > 0 && state.interval == 400 && state.sync == 1);

    a = hw_bemf_rel17_bridge_event(&state, true, true, 400, 0, 16);
    assert(!a.arm_delay && a.cancel_delay);
    assert(state.sync == 0 && state.fast == 0);
    assert(state.interval == 10000 && state.electrical_time == 100000000);

    a = hw_bemf_rel17_bridge_event(&state, false, false, 20000, 0, 16);
    assert(!a.arm_delay && !a.cancel_delay);
    puts("[PASS] rel17-owned BEMF bridge: ignore, capture, timeout, disabled");
    return 0;
}
