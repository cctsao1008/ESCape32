/*
 * AM13E FW1 BEMF timing policy lifted from ESCape32 src/main.c nextstep()
 * and iftim_isr(). Pure C; no GPIO/CMPSS/TIMG programming is performed.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

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
    /* Exact original ESCape32 seq[step-1] >> 6; selector is NOT a phase ID.
     * Hardware mapping to CMPSS and edge polarity is still outstanding.
     */
    static const uint8_t selector[6] = {5, 3, 6, 1, 7, 2};
    if (out == 0 || step < 1 || step > 6) return false;
    *out = selector[step-1] ^ (reverse ? 4U : 0U);
    return true;
}

static inline am13e_bemf_result_t am13e_bemf_on_capture(
    am13e_bemf_state_t *s, uint32_t captured_ticks,
    unsigned timing, uint32_t *delay_ticks)
{
    if (s == 0 || delay_ticks == 0 || timing > 32U) return AM13E_BEMF_IGNORED;
    if ((uint64_t)captured_ticks * 2U < s->interval)
        return AM13E_BEMF_IGNORED;

    uint64_t u = (uint64_t)s->interval * 3U;
    s->fast = (((uint64_t)captured_ticks * 4U < u ||
                (uint64_t)captured_ticks * 2U > u) &&
               s->electrical_time < 2000U);
    uint64_t next = ((uint64_t)captured_ticks + u) >> 2;
    if (next > UINT32_MAX) return AM13E_BEMF_IGNORED;
    s->interval = (uint32_t)next;
    uint32_t corrected = s->interval - (uint32_t)
        (((uint64_t)s->interval * timing) >> 5);
    uint32_t delay = corrected >> 1;
    *delay_ticks = delay != 0U ? delay : 1U;
    if (s->sync < 6U) ++s->sync;
    return AM13E_BEMF_ACCEPTED;
}

static inline void am13e_bemf_on_timeout(
    am13e_bemf_state_t *s, uint32_t xres)
{
    if (s == 0) return;
    s->sync = 0;
    s->fast = false;
    s->interval = 10000U << xres;
    s->electrical_time = 100000000U;
}
