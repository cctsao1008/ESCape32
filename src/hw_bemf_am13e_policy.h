/*
 * AM13E FW1 BEMF timing policy lifted from ESCape32 src/main.c nextstep()
 * and iftim_isr(). Pure C; no GPIO/CMPSS/TIMG programming is performed.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "hw_bemf_rel17_state.h"
#include "hw_six_step.h"

typedef struct {
    uint32_t interval;        /* ESCape32 ival, in IFTIM timer ticks */
    uint32_t electrical_time; /* ESCape32 ertm, existing units */
    uint8_t sync;
    bool fast;
} am13e_bemf_state_t;

typedef enum {
    AM13E_BEMF_IGNORED = 0,
    AM13E_BEMF_ACCEPTED = 1,
    AM13E_BEMF_TIMEOUT = 2
} am13e_bemf_result_t;

static inline bool am13e_bemf_selector(unsigned step, bool reverse, uint8_t *out)
{
    if (!out || step < 1U || step > 6U) return false;
    *out = (uint8_t)hw_six_step_decode(step, reverse).comparator;
    return true;
}

/*
 * Compatibility adapter for AM13E event scheduling.
 * The control decision is exclusively the rel17 state machine; AM13E owns
 * event delivery, delay arming and hardware clock-domain conversion.
 * Inputs are bounded to the signed int range used by ESCape32 rel17 math.
 */
static inline am13e_bemf_result_t am13e_bemf_on_capture(
    am13e_bemf_state_t *s, uint32_t captured_ticks,
    unsigned timing, uint32_t *delay_ticks)
{
    if (!s || !delay_ticks || timing > 32U ||
        captured_ticks > (uint32_t)INT_MAX ||
        s->interval > (uint32_t)(INT_MAX / 3) ||
        s->electrical_time > (uint32_t)INT_MAX)
        return AM13E_BEMF_IGNORED;
    const uint32_t triple = s->interval * 3U;
    if (captured_ticks > (uint32_t)INT_MAX - triple ||
        ((captured_ticks + triple) >> 2) >
            (uint32_t)(INT_MAX / (timing ? timing : 1U)))
        return AM13E_BEMF_IGNORED;

    hw_bemf_rel17_state_t rel17 = {
        .interval = (int)s->interval,
        .electrical_time = (int)s->electrical_time,
        .sync = s->sync,
        .fast = s->fast
    };
    const hw_bemf_rel17_event_t event = hw_bemf_rel17_process(
        &rel17, 0, 1, (int)captured_ticks, 0, (int)timing);
    if (event.action != HW_BEMF_REL17_CAPTURE)
        return AM13E_BEMF_IGNORED;
    s->interval = (uint32_t)rel17.interval;
    s->electrical_time = (uint32_t)rel17.electrical_time;
    s->sync = (uint8_t)rel17.sync;
    s->fast = rel17.fast != 0;
    *delay_ticks = (uint32_t)event.delay;
    return AM13E_BEMF_ACCEPTED;
}

static inline void am13e_bemf_on_timeout(
    am13e_bemf_state_t *s, uint32_t xres)
{
    if (!s || xres > 17U) return;
    hw_bemf_rel17_state_t rel17 = {
        .interval = (int)s->interval,
        .electrical_time = (int)s->electrical_time,
        .sync = s->sync,
        .fast = s->fast
    };
    (void)hw_bemf_rel17_process(&rel17, 1, 0, 0, (int)xres, 0);
    s->interval = (uint32_t)rel17.interval;
    s->electrical_time = (uint32_t)rel17.electrical_time;
    s->sync = (uint8_t)rel17.sync;
    s->fast = rel17.fast != 0;
}
