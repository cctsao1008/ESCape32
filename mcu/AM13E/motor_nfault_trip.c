/* AM13E reference: PB15 nFAULT (GPIO47) -> INPUTXBAR2 -> PWMXBAR1 -> MCPWM0 OST1.
 * This is an asynchronous driver-fault hardware interlock, distinct from
 * independent over-current sensing. Never touches PB13 or motor pad mux.
 * Software must not clear the latched OST while nFAULT is unqualified.
 */
#include <stdint.h>
#include <soc.h>
#include <dl_xbar.h>
#include <dl_mcpwm.h>
#include "motor_backend.h"
#include "motor_nfault_trip.h"
#include "fault_trip_backend.h"

#define AM13E_REF_NFAULT_GPIO_INDEX 47U
#define AM13E_REF_NFAULT_INPUTXBAR DL_XBAR_INPUT2
#define AM13E_REF_NFAULT_PWMXBAR DL_XBAR_TRIP1
#define AM13E_REF_NFAULT_TZ_SIGNAL DL_MCPWM_TZ_SIGNAL_OST1
#define AM13E_REF_NFAULT_SOURCE DL_XBAR_PWM_INPUTXBAR2

_Static_assert(AM13E_REF_NFAULT_SOURCE == 0x15U &&
               AM13E_REF_NFAULT_TZ_SIGNAL == MCPWM_TZSEL_OST1_MASK &&
               IOMUX_PINCM_PB15 == 47U,
               "AM13E reference PB15 nFAULT hardware TZ routing changed");

/* Reference routing is deliberately owned by this board adapter. */
static const AM13E_FaultTripRoute reference_nfault_route = {
    .mcpwm = MCPWM0,
    .gpio_index = AM13E_REF_NFAULT_GPIO_INDEX,
    .input_xbar = AM13E_REF_NFAULT_INPUTXBAR,
    .pwm_trip = AM13E_REF_NFAULT_PWMXBAR,
    .pwm_source = AM13E_REF_NFAULT_SOURCE,
    .ost_signal = AM13E_REF_NFAULT_TZ_SIGNAL,
    .ost_flag = DL_MCPWM_TZ_FLAG_OST_TZ1,
    .active_low = true
};

static volatile uint32_t hardware_trip_installed;

int am13e_app_motor_nfault_trip_ready(void)
{
    return hardware_trip_installed &&
           am13e_mcu_fault_trip_ready(&reference_nfault_route);
}

void am13e_app_motor_nfault_trip_init(void)
{
    if (__get_PRIMASK() == 0U || hardware_trip_installed)
        am13e_app_motor_fault_reset();
    /* A low nFAULT is allowed to latch OST, not cleared. Boot and
     * GPIO1/PB15 ownership, motor gate fail-closed policy unchanged.
     */
    am13e_mcu_fault_trip_install(&reference_nfault_route);
    hardware_trip_installed = 1U;
    if (!am13e_app_motor_nfault_trip_ready())
        am13e_app_motor_fault_reset();
}
