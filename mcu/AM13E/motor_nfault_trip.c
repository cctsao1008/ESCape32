/* E62: PB15 nFAULT (GPIO47) -> INPUTXBAR2 -> PWMXBAR1 -> MCPWM0 OST1.
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

#define E62_NFAULT_GPIO_INDEX 47U
#define E62_NFAULT_INPUTXBAR DL_XBAR_INPUT2
#define E62_NFAULT_PWMXBAR DL_XBAR_TRIP1
#define E62_NFAULT_TZ_SIGNAL DL_MCPWM_TZ_SIGNAL_OST1
#define E62_NFAULT_SOURCE DL_XBAR_PWM_INPUTXBAR2

_Static_assert(E62_NFAULT_SOURCE == 0x15U &&
               E62_NFAULT_TZ_SIGNAL == MCPWM_TZSEL_OST1_MASK &&
               IOMUX_PINCM_PB15 == 47U,
               "E62 PB15 nFAULT hardware TZ routing changed");

static volatile uint32_t hardware_trip_installed;

int am13e_app_motor_nfault_trip_ready(void)
{
    const uint32_t selected=PWMXBAR->PWM_XBAR_GXSEL[0].PWMXBARG0SEL;
    const uint32_t tzflags=DL_MCPWM_getTripZoneFlagStatus(MCPWM0);
    return hardware_trip_installed &&
           INPUTXBAR->INPUTSELECT[1] == E62_NFAULT_GPIO_INDEX &&
           (selected & (UINT32_C(1) << E62_NFAULT_SOURCE)) != 0U &&
           (PWMXBAR->PWMXBAROUTINVERT & UINT32_C(1)) != 0U &&
           (MCPWM0->TZSEL & E62_NFAULT_TZ_SIGNAL) != 0U &&
           (tzflags & DL_MCPWM_TZ_FLAG_OST_TZ1) == 0U &&
           (MCPWM0->TZCTL & (MCPWM_TZCTL_TZA_MASK | MCPWM_TZCTL_TZB_MASK)) == 0U;
}

void am13e_app_motor_nfault_trip_init(void)
{
    if (__get_PRIMASK() == 0U || hardware_trip_installed)
        am13e_app_motor_fault_reset();
    /* GPIO1/PB15 digital fault input must already be enabled by initgpio.
     * A low nFAULT is deliberately ALLOWED to latch OST, not cleared.
     */
    DL_XBAR_enableRawInput(E62_NFAULT_GPIO_INDEX);
    DL_XBAR_setInputXBAR(E62_NFAULT_INPUTXBAR,E62_NFAULT_GPIO_INDEX);
    DL_XBAR_clearPWMXBARSourceSelection(E62_NFAULT_PWMXBAR);
    DL_XBAR_selectPWMXBARSource(E62_NFAULT_PWMXBAR,E62_NFAULT_SOURCE);
    DL_XBAR_invertPWMXBARSignal(E62_NFAULT_PWMXBAR,true);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZA,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZB,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_enableTripZoneSignals(MCPWM0,E62_NFAULT_TZ_SIGNAL);
    hardware_trip_installed=1U;
    if (!am13e_app_motor_nfault_trip_ready())
        am13e_app_motor_fault_reset();
}
