#include <assert.h>
#include "hw_bemf_rel17_state.h"

int main(void)
{
    hw_bemf_rel17_state_t s = {4000, 1500, 5, 1};
    hw_bemf_rel17_event_t e = hw_bemf_rel17_process(&s, 0, 0, 9000, 2, 16);
    assert(e.action == HW_BEMF_REL17_IGNORE && s.interval == 4000);
    e = hw_bemf_rel17_process(&s, 0, 1, 1999, 2, 16);
    assert(e.action == HW_BEMF_REL17_IGNORE && s.sync == 5);
    e = hw_bemf_rel17_process(&s, 0, 1, 2000, 2, 16);
    assert(e.action == HW_BEMF_REL17_CAPTURE);
    assert(s.interval == 3500 && s.sync == 6);
    assert(e.delay == 875);
    e = hw_bemf_rel17_process(&s, 0, 1, 4000, 2, 16);
    assert(e.action == HW_BEMF_REL17_CAPTURE && s.sync == 6);
    e = hw_bemf_rel17_process(&s, 1, 1, 10000, 2, 16);
    assert(e.action == HW_BEMF_REL17_TIMEOUT);
    assert(s.sync == 0 && s.fast == 0 && s.interval == 40000);
    assert(s.electrical_time == 100000000);
    e = hw_bemf_rel17_process(&s, 0, 1, 1, 2, 16);
    assert(e.action == HW_BEMF_REL17_IGNORE);
    return 0;
}
