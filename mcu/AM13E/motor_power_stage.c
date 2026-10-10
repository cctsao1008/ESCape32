/* TI AM13E23019 MCPWM0 six-pad/power-stage hardware owner.
 *
 * Full reference firmware compiles this LIVE MCU-side lifecycle with
 * G431-derived software defaults. These are NOT actual PCB measurements.
 *
 * PB15 nFAULT already owns OST1 via INPUTXBAR2/PWMXBAR1. Independent
 * overcurrent, if mapped, owns INPUTXBAR3/PWMXBAR2/OST2. Independent
 * OC PINCM=0 disables that optional trip; mandatory PB15/OST1 stays.
 * PB14 ECAP0/DShot stays untouched.
 *
 * HW safety ordering:
 *   boot: PB13 INACTIVE -> GPIO output, six pads INPUT, OST1 installed
 *   start: require OST1 (+optional OST2) healthy, configured RED/FED
 *          -> mux six MCPWM outputs
 *          -> release AQ software force -> PB13 ACTIVE last
 *   stop/fault: PB13 INACTIVE first -> force AQ LOW -> six pads INPUT.
 * All calls execute within a PRIMASK-protected motor critical section.
 */
#include "motor_power_stage.h"
#include "board_reference_io.h"
#include "board_configuration.h"
#include "motor_output_backend.h"
#include "board_motor_output_provider.h"
#include "motor_fault_route.h"
#include "fault_input.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_xbar.h>
#include <dl_mcpwm.h>
#include <stdint.h>

#define GATE_EN_PIN DL_GPIO_PIN(13U)
#define OC_SOURCE DL_XBAR_PWM_INPUTXBAR3
#define OC_TRIP    DL_XBAR_TRIP2
#define OC_SIGNAL  DL_MCPWM_TZ_SIGNAL_OST2

#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
#if AM13E_BOARD_POWER_STAGE_PROFILE != 1
#error "AM13E_BOARD_POWER_STAGE_PROFILE must equal 1"
#endif
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
#error "Physical output requires compiled RED/FED control"
#endif
#ifndef AM13E_BOARD_SENSORS_CONFIGURED
#error "Physical output requires compiled ADC/NTC scaling path"
#endif
#if !defined(AM13E_BOARD_PB13_ACTIVE_LEVEL) || \
    !defined(AM13E_BOARD_GATE_PWM_INVERT_MASK) || \
    !defined(AM13E_BOARD_OC_GPIO_PINCM) || \
    !defined(AM13E_BOARD_OC_ACTIVE_LOW) || \
    !defined(AM13E_BOARD_GATE_INPUTS_HIZ_SAFE) || \
    !defined(AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN)
#error "No guessed gate/OC configuration: all six board fields required"
#endif
_Static_assert((AM13E_BOARD_PB13_ACTIVE_LEVEL==0 ||
                AM13E_BOARD_PB13_ACTIVE_LEVEL==1) &&
               AM13E_BOARD_GATE_PWM_INVERT_MASK>=0 &&
               AM13E_BOARD_GATE_PWM_INVERT_MASK<=63 &&
               (AM13E_BOARD_OC_ACTIVE_LOW==0 ||
                AM13E_BOARD_OC_ACTIVE_LOW==1) &&
               (AM13E_BOARD_GATE_INPUTS_HIZ_SAFE==0 ||
                AM13E_BOARD_GATE_INPUTS_HIZ_SAFE==1) &&
               (AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN==0 ||
                AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN==1) &&
               AM13E_BOARD_OC_GPIO_PINCM>=0 &&
               AM13E_BOARD_OC_GPIO_PINCM<64 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PB13 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PB14 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PB15 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA8 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA11 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA9 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA30 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA10 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA31 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA17 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA4 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA3 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA2 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA16 &&
               AM13E_BOARD_OC_GPIO_PINCM!=IOMUX_PINCM_PA18,
               "Invalid/conflicting AM13E power-stage pin/level configuration");
#endif

static volatile uint32_t initialized;
static volatile uint32_t attached;

int am13e_power_stage_board_profile_present(void)
{
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    return 1;
#else
    return 0;
#endif
}

#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
static void driver_enable_level(int active)
{
    const int high=active ? AM13E_BOARD_PB13_ACTIVE_LEVEL :
                            !AM13E_BOARD_PB13_ACTIVE_LEVEL;
    if(high) DL_GPIO_setPins(GPIO1,GATE_EN_PIN);
    else DL_GPIO_clearPins(GPIO1,GATE_EN_PIN);
}
static int pwm_function_readback(void)
{
    return am13e_mcu_motor_pads_pwm_matches(
        am13e_board_motor_pad_route(), AM13E_BOARD_GATE_PWM_INVERT_MASK);
}
static int driver_level_matches(int active)
{
    const int high=active ? AM13E_BOARD_PB13_ACTIVE_LEVEL :
                            !AM13E_BOARD_PB13_ACTIVE_LEVEL;
    return (GPIO1->DOE31_0&GATE_EN_PIN)!=0U &&
           (((GPIO1->DOUT31_0&GATE_EN_PIN)!=0U)==(high!=0));
}
static void set_pwm_force(DL_MCPWM_ACTION_QUALIFIER_SW_FORCE_OUTPUT force)
{
    for(unsigned i=0U;i<6U;++i)
        DL_MCPWM_setActionQualifierSWAction(MCPWM0,
            (DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE)i,force);
}
#endif

int am13e_power_stage_oc_trip_ready(void)
{
#if defined(AM13E_BOARD_POWER_STAGE_PROFILE) && AM13E_BOARD_OC_GPIO_PINCM != 0
    const uint32_t selected=PWMXBAR->PWM_XBAR_GXSEL[1].PWMXBARG0SEL;
    const uint32_t tzflag=DL_MCPWM_getTripZoneFlagStatus(MCPWM0);
    const uint32_t inverted=PWMXBAR->PWMXBAROUTINVERT & (1U<<1U);
    return initialized &&
           INPUTXBAR->INPUTSELECT[2]==AM13E_BOARD_OC_GPIO_PINCM &&
           (selected & (1U<<(unsigned)OC_SOURCE))!=0U &&
           (inverted!=0U)==(AM13E_BOARD_OC_ACTIVE_LOW!=0) &&
           (MCPWM0->TZSEL & OC_SIGNAL)!=0U &&
           (tzflag & DL_MCPWM_TZ_FLAG_OST_TZ2)==0U;
#else
    return 0;
#endif
}

void am13e_power_stage_init(void)
{
    if (__get_PRIMASK()!=1U || initialized) {
        for(;;){ __NOP(); } /* Programmer/ownership fault: fail closed */
    }
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    /* Set inactive PB13 data BEFORE enabling its digital output.
     * Never connect PWM pins until independent OST1 and OST2 exist.
     */
    DL_GPIO_enablePower(GPIO1);
    if(!DL_GPIO_isPowerEnabled(GPIO1)) for(;;){__NOP();}
    driver_enable_level(0);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB13);
    DL_GPIO_enableOutput(GPIO1,GATE_EN_PIN);
#if AM13E_BOARD_OC_GPIO_PINCM != 0
    DL_GPIO_initDigitalInput(AM13E_BOARD_OC_GPIO_PINCM);
    DL_XBAR_enableRawInput(AM13E_BOARD_OC_GPIO_PINCM);
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT3,AM13E_BOARD_OC_GPIO_PINCM);
    DL_XBAR_clearPWMXBARSourceSelection(OC_TRIP);
    DL_XBAR_selectPWMXBARSource(OC_TRIP,OC_SOURCE);
    DL_XBAR_invertPWMXBARSignal(OC_TRIP,AM13E_BOARD_OC_ACTIVE_LOW!=0);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZA,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZB,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_enableTripZoneSignals(MCPWM0,OC_SIGNAL);
#endif /* Independent OC route assigned */
#endif
    attached=0U;
    initialized=1U;
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    if (!driver_level_matches(0) ||
        !am13e_app_motor_nfault_trip_ready() ||
        (AM13E_BOARD_OC_GPIO_PINCM != 0 &&
         !am13e_power_stage_oc_trip_ready()))
        for(;;){__NOP();}
#endif
}

void am13e_power_stage_force_off(void)
{
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    if(initialized) {
        driver_enable_level(0);
        __DSB();
        set_pwm_force(DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
        if (!am13e_mcu_motor_pads_disconnect(am13e_board_motor_pad_route()))
            for (;;) { __NOP(); } /* Never ignore a failed safe disconnect. */
    }
#endif
    attached=0U;
}

int am13e_power_stage_attached(void)
{
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    return initialized && attached && driver_level_matches(1) &&
           am13e_mcu_motor_pads_gpio_oe_off(am13e_board_motor_pad_route()) &&
           pwm_function_readback() && !am13e_app_nfault_asserted() &&
           am13e_app_motor_nfault_trip_ready() &&
           (AM13E_BOARD_OC_GPIO_PINCM == 0 ||
            am13e_power_stage_oc_trip_ready());
#else
    return 0;
#endif
}

int am13e_power_stage_attach(void)
{
#ifdef AM13E_BOARD_POWER_STAGE_PROFILE
    if (!initialized || attached ||
        __get_PRIMASK()!=1U || !driver_level_matches(0) ||
        !am13e_app_motor_nfault_trip_ready() ||
        (AM13E_BOARD_OC_GPIO_PINCM != 0 &&
         !am13e_power_stage_oc_trip_ready()) ||
        am13e_app_nfault_asserted() ||
        (MCPWM0->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)!=
             (uint32_t)DL_MCPWM_COUNTER_MODE_UP ||
        (MCPWM0->PWM1_AQSFRC & 0x33U)!=0x11U ||
        (MCPWM0->PWM2_AQSFRC & 0x33U)!=0x11U ||
        (MCPWM0->PWM3_AQSFRC & 0x33U)!=0x11U)
        return 0;
    /* External driver still disabled; only now change all six IOMUXes. */
    if (!am13e_mcu_motor_pads_connect_pwm(am13e_board_motor_pad_route(),
                                           AM13E_BOARD_GATE_PWM_INVERT_MASK)) {
        am13e_power_stage_force_off();
        return 0;
    }
    if(!pwm_function_readback()) {am13e_power_stage_force_off();return 0;}
    set_pwm_force(DL_MCPWM_AQ_SW_FORCE_DISABLED);
    __DSB();
    __ISB();
    driver_enable_level(1);
    attached=1U;
    if (!am13e_power_stage_attached()) {
        am13e_power_stage_force_off();
        return 0;
    }
    return 1;
#else
    return 0; /* Explicit output-disabled diagnostic build only. */
#endif
}
