#include "fault_trip_backend.h"
#include "fault_trip_route_plan.h"
#include <soc.h>
#include <stddef.h>

_Static_assert(DL_XBAR_PWM_INPUTXBAR1 == 0x14U &&
               DL_XBAR_PWM_INPUTXBAR12 == 0x1FU &&
               DL_XBAR_PWM_INPUTXBAR13 == 0x100U &&
               DL_XBAR_PWM_INPUTXBAR16 == 0x103U &&
               DL_MCPWM_TZ_SIGNAL_OST1 == 0x10000U &&
               DL_MCPWM_TZ_FLAG_OST_TZ1 == 0x10000U,
               "AM13E TI SDK route encodings changed");

int am13e_mcu_fault_trip_route_valid(const AM13E_FaultTripRoute *route)
{
    if (route == NULL ||
        (route->mcpwm != MCPWM0 && route->mcpwm != MCPWM1 &&
         route->mcpwm != MCPWM2 && route->mcpwm != MCPWM3 &&
         route->mcpwm != MCPWM4)) {
        return 0;
    }
    return am13e_fault_trip_route_fields_valid(
        route->gpio_index, (unsigned)route->input_xbar,
        (unsigned)route->pwm_trip, (unsigned)route->pwm_source,
        route->ost_signal, route->ost_flag);
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
