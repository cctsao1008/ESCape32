#include "fault_trip_backend.h"
#include <soc.h>
#include <stddef.h>

int am13e_mcu_fault_trip_route_valid(const AM13E_FaultTripRoute *route)
{
    if (route == NULL ||
        (route->mcpwm != MCPWM0 && route->mcpwm != MCPWM1 &&
         route->mcpwm != MCPWM2 && route->mcpwm != MCPWM3 &&
         route->mcpwm != MCPWM4) ||
        route->gpio_index >= 107U ||
        route->input_xbar > DL_XBAR_INPUT16 ||
        route->pwm_trip > DL_XBAR_TRIP8) {
        return 0;
    }
    /* INPUTXBAR1..12: PWMXBAR group 0; 13..16: group 1. The
     * physical input and source must be the same explicit route.
     */
    const uint32_t input = (uint32_t)route->input_xbar;
    const uint32_t expected_source =
        input < 12U ? (uint32_t)DL_XBAR_PWM_INPUTXBAR1 + input :
                      (uint32_t)DL_XBAR_PWM_INPUTXBAR13 + input - 12U;
    const uint32_t trip = (uint32_t)route->pwm_trip;
    return (uint32_t)route->pwm_source == expected_source &&
           route->ost_signal == (DL_MCPWM_TZ_SIGNAL_OST1 << trip) &&
           route->ost_flag == (DL_MCPWM_TZ_FLAG_OST_TZ1 << trip);
}

static void trip_fail_closed(void)
{
    __disable_irq();
    for (;;) { __NOP(); }
}

void am13e_mcu_fault_trip_install(const AM13E_FaultTripRoute *route)
{
    if (!am13e_mcu_fault_trip_route_valid(route) ||
        __get_PRIMASK() != 1U) {
        trip_fail_closed();
    }
    DL_XBAR_enableRawInput(route->gpio_index);
    DL_XBAR_setInputXBAR(route->input_xbar, route->gpio_index);
    DL_XBAR_clearPWMXBARSourceSelection(route->pwm_trip);
    DL_XBAR_selectPWMXBARSource(route->pwm_trip, route->pwm_source);
    DL_XBAR_invertPWMXBARSignal(route->pwm_trip, route->active_low);
    DL_MCPWM_setTripZoneAction(route->mcpwm,
                              DL_MCPWM_TZ_ACTION_EVENT_TZA,
                              DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_setTripZoneAction(route->mcpwm,
                              DL_MCPWM_TZ_ACTION_EVENT_TZB,
                              DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_enableTripZoneSignals(route->mcpwm, route->ost_signal);
}

int am13e_mcu_fault_trip_ready(const AM13E_FaultTripRoute *route)
{
    if (!am13e_mcu_fault_trip_route_valid(route)) return 0;
    const uint32_t trip = (uint32_t)route->pwm_trip;
    const uint32_t source = (uint32_t)route->pwm_source;
    const uint32_t bit = UINT32_C(1) << (source & 0xFFU);
    const uint32_t selected =
        source >> 8U ? PWMXBAR->PWM_XBAR_GXSEL[trip].PWMXBARG1SEL :
                       PWMXBAR->PWM_XBAR_GXSEL[trip].PWMXBARG0SEL;
    const uint32_t flags = DL_MCPWM_getTripZoneFlagStatus(route->mcpwm);
    return INPUTXBAR->INPUTSELECT[(uint32_t)route->input_xbar] ==
               route->gpio_index &&
           (selected & bit) != 0U &&
           ((PWMXBAR->PWMXBAROUTINVERT & (UINT32_C(1) << trip)) != 0U) ==
               route->active_low &&
           (route->mcpwm->TZSEL & route->ost_signal) != 0U &&
           (flags & route->ost_flag) == 0U &&
           (route->mcpwm->TZCTL &
               (MCPWM_TZCTL_TZA_MASK | MCPWM_TZCTL_TZB_MASK)) == 0U;
}
