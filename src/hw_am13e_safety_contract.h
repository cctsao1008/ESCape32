/* E62 AM13E software-side qualification gates for motor-control integration.
 * Pure C: does not imply board qualification or enable any output.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "hw_motor_am13e_commutation.h"

typedef struct {
    uint32_t core_hz;
    uint32_t mcpwm_hz;
    uint32_t ecap_hz;
    uint32_t timg12_hz;
    uint32_t required_deadtime_ns;
    bool phase_map_verified;
    bool gate_driver_verified;
    bool trip_path_verified;
    bool comparator_route_verified;
    bool ecap_epoch_verified;
    bool output_states_verified;
} am13e_motor_contract_t;

typedef enum {
    AM13E_MOTOR_UNQUALIFIED = 0,
    AM13E_MOTOR_DISARMED,
    AM13E_MOTOR_ARMED,
    AM13E_MOTOR_FAULT_LATCHED
} am13e_motor_state_t;

typedef struct {
    am13e_motor_state_t state;
    bool throttle_zero;
    bool fault_present;
} am13e_motor_guard_t;

static inline bool am13e_clock_convert_ticks(uint32_t ticks,
    uint32_t source_hz, uint32_t target_hz, uint32_t *out)
{
    if (!out || !ticks || !source_hz || !target_hz) return false;
    /* Conservative round-up: never schedule earlier than the requested time. */
    /* Full uint32 products fit uint64, but adding source_hz - 1 can
     * overflow. Quotient plus nonzero remainder implements ceil safely.
     */
    const uint64_t product = (uint64_t)ticks * (uint64_t)target_hz;
    uint64_t scaled = product / source_hz;
    if (product % source_hz) ++scaled;
    if (!scaled || scaled > UINT32_MAX) return false;
    *out = (uint32_t)scaled;
    return true;
}

/* MCPWM period counter ticks for edge-aligned PWM, verified clock input.
 * Reject zero, truncation and timer-width overflow. No board clock assumed.
 */
static inline bool am13e_mcpwm_period_ticks(uint32_t mcpwm_hz,
    uint32_t pwm_hz, uint16_t *period)
{
    if (!period || !mcpwm_hz || !pwm_hz ||
        (mcpwm_hz % pwm_hz) != 0U) return false;
    uint32_t ticks = mcpwm_hz / pwm_hz;
    if (ticks < 2U || ticks > UINT16_MAX) return false;
    *period = (uint16_t)ticks;
    return true;
}

static inline bool am13e_contract_qualified(const am13e_motor_contract_t *c)
{
    return c && c->core_hz && c->mcpwm_hz && c->ecap_hz &&
        c->timg12_hz && c->required_deadtime_ns &&
        c->phase_map_verified && c->gate_driver_verified &&
        c->trip_path_verified && c->comparator_route_verified &&
        c->ecap_epoch_verified && c->output_states_verified;
}

static inline void am13e_guard_init(am13e_motor_guard_t *g,
                                    const am13e_motor_contract_t *c)
{
    if (!g) return;
    g->state = am13e_contract_qualified(c)
        ? AM13E_MOTOR_DISARMED : AM13E_MOTOR_UNQUALIFIED;
    g->throttle_zero = false;
    g->fault_present = false;
}

static inline void am13e_guard_latch_fault(am13e_motor_guard_t *g)
{
    if (!g) return;
    g->fault_present = true;
    g->state = AM13E_MOTOR_FAULT_LATCHED;
}

/* Requalification plus zero throttle and explicit reset are required after trip. */
static inline bool am13e_guard_clear_fault(am13e_motor_guard_t *g,
    const am13e_motor_contract_t *c, bool fault_input_inactive)
{
    if (!g || !fault_input_inactive || !g->throttle_zero ||
        !am13e_contract_qualified(c)) return false;
    g->fault_present = false;
    g->state = AM13E_MOTOR_DISARMED;
    return true;
}

static inline bool am13e_guard_arm(am13e_motor_guard_t *g)
{
    if (!g || g->state != AM13E_MOTOR_DISARMED ||
        g->fault_present || !g->throttle_zero) return false;
    g->state = AM13E_MOTOR_ARMED;
    return true;
}

static inline bool am13e_guard_commutate(const am13e_motor_guard_t *g,
    unsigned positive, unsigned negative, bool damp,
    am13e_commutation_plan_t *plan)
{
    return g && g->state == AM13E_MOTOR_ARMED &&
        !g->fault_present &&
        am13e_commutation_plan(positive, negative, damp, plan);
}
