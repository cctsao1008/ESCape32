#include "fault_trip_route_plan.h"

int am13e_fault_trip_route_fields_valid(unsigned gpio_index,
                                         unsigned input_xbar,
                                         unsigned pwm_trip,
                                         unsigned pwm_source,
                                         uint32_t ost_signal,
                                         uint32_t ost_flag)
{
    /* 16 INPUTXBAR inputs, 8 MCPWM trips, 107 GPIO indices.
     * These are silicon-level limits, NOT a package pinout.
     */
    if (gpio_index >= 107U || input_xbar >= 16U || pwm_trip >= 8U)
        return 0;
    const unsigned source = input_xbar < 12U ?
        0x14U + input_xbar : 0x100U + input_xbar - 12U;
    return pwm_source == source &&
           ost_signal == (UINT32_C(0x10000) << pwm_trip) &&
           ost_flag == (UINT32_C(0x10000) << pwm_trip);
}
