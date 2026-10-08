#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "hw_am13e_safety_contract.h"

int main(void)
{
    uint32_t ticks = 0;
    assert(!am13e_clock_convert_ticks(100, 0, 1000000, &ticks));
    assert(am13e_clock_convert_ticks(150, 3000000, 1000000, &ticks));
    assert(ticks == 50);
    assert(am13e_clock_convert_ticks(1, 3000000, 1000000, &ticks));
    assert(ticks == 1);
    assert(!am13e_clock_convert_ticks(UINT32_MAX, 1, UINT32_MAX, &ticks));

    uint16_t period = 0;
    assert(!am13e_mcpwm_period_ticks(0, 24000, &period));
    assert(!am13e_mcpwm_period_ticks(80000000, 0, &period));
    assert(!am13e_mcpwm_period_ticks(80000000, 24000, &period));
    assert(am13e_mcpwm_period_ticks(80000000, 20000, &period));
    assert(period == 4000);
    assert(!am13e_mcpwm_period_ticks(80000000, 1, &period));
    assert(!am13e_mcpwm_period_ticks(1000, 1000, &period));
    am13e_motor_contract_t c = {0};
    am13e_motor_guard_t g;
    am13e_guard_init(&g, &c);
    assert(g.state == AM13E_MOTOR_UNQUALIFIED);
    assert(!am13e_guard_arm(&g));
    c.core_hz=160000000; c.mcpwm_hz=80000000;
    c.ecap_hz=80000000; c.timg12_hz=40000000;
    c.required_deadtime_ns=300;
    c.phase_map_verified=true; c.gate_driver_verified=true;
    c.trip_path_verified=true; c.comparator_route_verified=true;
    c.ecap_epoch_verified=true; c.output_states_verified=true;
    assert(am13e_contract_qualified(&c));
    am13e_guard_init(&g, &c);
    assert(g.state == AM13E_MOTOR_DISARMED);
    assert(!am13e_guard_arm(&g));
    g.throttle_zero=true;
    assert(am13e_guard_arm(&g));
    am13e_commutation_plan_t plan;
    assert(am13e_guard_commutate(&g, 1, 2, true, &plan));
    assert(!am13e_guard_commutate(&g, 1, 1, true, &plan));
    assert(!am13e_guard_commutate(&g, 3, 4, false, &plan));
    am13e_guard_latch_fault(&g);
    assert(g.state == AM13E_MOTOR_FAULT_LATCHED);
    assert(!am13e_guard_commutate(&g, 1, 2, false, &plan));
    assert(!am13e_guard_clear_fault(&g, &c, false));
    g.throttle_zero=false;
    assert(!am13e_guard_clear_fault(&g, &c, true));
    g.throttle_zero=true;
    assert(am13e_guard_clear_fault(&g, &c, true));
    assert(g.state == AM13E_MOTOR_DISARMED);
    puts("[PASS] AM13E clock units, 6-step qualification, fault latch and rearm");
    return 0;
}
