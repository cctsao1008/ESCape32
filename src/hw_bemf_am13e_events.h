/* E62 FW1 event boundary: actual BEMF samples -> deferred commutation.
 * Peripheral-independent handler, with injectable timer and commutation ops.
 * No physical PWM outputs are enabled here.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "hw_bemf_am13e_policy.h"

typedef struct {
    void (*arm_delay)(void *user, uint32_t ticks);
    void (*cancel_delay)(void *user);
    void (*commutation_due)(void *user);
    void *user;
} am13e_bemf_event_ops_t;

typedef struct {
    am13e_bemf_state_t policy;
    am13e_bemf_event_ops_t ops;
    unsigned timing;
    bool pending;
} am13e_bemf_event_engine_t;

static inline bool am13e_bemf_event_init(
    am13e_bemf_event_engine_t *e, const am13e_bemf_event_ops_t *ops,
    am13e_bemf_state_t initial, unsigned timing)
{
    if (!e || !ops || !ops->arm_delay || !ops->cancel_delay ||
        !ops->commutation_due || timing > 32U) return false;
    e->policy = initial;
    e->ops = *ops;
    e->timing = timing;
    e->pending = false;
    return true;
}

static inline am13e_bemf_result_t am13e_bemf_event_capture(
    am13e_bemf_event_engine_t *e, uint32_t capture_ticks)
{
    uint32_t delay = 0;
    if (!e || e->pending) return AM13E_BEMF_IGNORED;
    am13e_bemf_result_t result = am13e_bemf_on_capture(
        &e->policy, capture_ticks, e->timing, &delay);
    if (result == AM13E_BEMF_ACCEPTED) {
        e->pending = true;
        e->ops.arm_delay(e->ops.user, delay);
    }
    return result;
}

static inline void am13e_bemf_event_delay_elapsed(
    am13e_bemf_event_engine_t *e)
{
    if (!e || !e->pending) return;
    e->pending = false;
    e->ops.commutation_due(e->ops.user);
}

static inline void am13e_bemf_event_timeout(
    am13e_bemf_event_engine_t *e, unsigned xres)
{
    if (!e) return;
    e->pending = false;
    e->ops.cancel_delay(e->ops.user);
    am13e_bemf_on_timeout(&e->policy, xres);
}
