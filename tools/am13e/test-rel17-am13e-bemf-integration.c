/* AM13E event engine must use the same rel17 policy decisions. */
#include <assert.h>
#include <stdint.h>
#include "hw_bemf_am13e_events.h"
#include "hw_six_step.h"

typedef struct { unsigned armed, canceled, due; uint32_t ticks; } fixture_t;
static void arm(void *ctx, uint32_t t)
{
    fixture_t *f = ctx;
    ++f->armed;
    f->ticks = t;
}
static void cancel(void *ctx) { ++((fixture_t *)ctx)->canceled; }
static void due(void *ctx) { ++((fixture_t *)ctx)->due; }

int main(void)
{
    for (unsigned reverse = 0; reverse < 2; ++reverse)
        for (unsigned step = 1; step <= 6; ++step) {
            uint8_t selector = 0;
            assert(am13e_bemf_selector(step, reverse != 0, &selector));
            assert(selector == hw_six_step_decode(step, reverse).comparator);
        }

    fixture_t f = {0};
    const am13e_bemf_event_ops_t ops = {arm, cancel, due, &f};
    am13e_bemf_event_engine_t e;
    const am13e_bemf_state_t initial = {
        .interval = 1000, .electrical_time = 1500, .sync = 5, .fast = false
    };
    assert(am13e_bemf_event_init(&e, &ops, initial, 16));
    assert(am13e_bemf_event_capture(&e, 499) == AM13E_BEMF_IGNORED);
    assert(e.policy.sync == 5 && f.armed == 0);
    assert(am13e_bemf_event_capture(&e, 500) == AM13E_BEMF_ACCEPTED);
    assert(e.policy.interval == 875 && e.policy.sync == 6);
    assert(f.armed == 1 && f.ticks == 218 && e.pending);
    assert(am13e_bemf_event_capture(&e, 500) == AM13E_BEMF_IGNORED);
    am13e_bemf_event_delay_elapsed(&e);
    assert(f.due == 1 && !e.pending);
    am13e_bemf_event_timeout(&e, 2);
    assert(f.canceled == 1 && e.policy.interval == 40000);
    assert(e.policy.sync == 0 && !e.policy.fast);
    am13e_bemf_event_delay_elapsed(&e);
    assert(f.due == 1);
    return 0;
}
