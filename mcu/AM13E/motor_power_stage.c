/* TI AM13E23019 MCPWM0 six-pad/power-stage hardware owner.
 *
 * Full MCU-side lifecycle is implemented, but *not enabled* on an
 * unqualified E62 board. All compile-time values below must come from
 * a reviewed electrical design, NEVER a TI EVM or guessed schematic.
 *
 * PB15 nFAULT already owns OST1 via INPUTXBAR2/PWMXBAR1. Independent
 * overcurrent owns INPUTXBAR3/PWMXBAR2/OST2 with a separate physical
 * active-high/low input. PB14 ECAP0/DShot stays untouched.
 *
 * HW safety ordering:
 *   boot: PB13 INACTIVE -> GPIO output, six pads INPUT, both OST active
 *   start: require both OST healthy & DB profile -> mux six MCPWM outputs
 *          -> release AQ software force -> PB13 ACTIVE last
 *   stop/fault: PB13 INACTIVE first -> force AQ LOW -> six pads INPUT.
 * All calls execute within a PRIMASK-protected motor critical section.
 */
#include "motor_power_stage.h"
#include "board_io_plan_v1.h"
#include "motor_nfault_trip.h"
#include "gpio_runtime.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_xbar.h>
#include <dl_mcpwm.h>
#include <stdint.h>

#define GATE_EN_PIN DL_GPIO_PIN(13U)
#define PWM_PADS (DL_GPIO_PIN(8U)|DL_GPIO_PIN(11U)|DL_GPIO_PIN(9U)| \
                  DL_GPIO_PIN(30U)|DL_GPIO_PIN(10U)|DL_GPIO_PIN(31U))
#define OC_SOURCE DL_XBAR_PWM_INPUTXBAR3
#define OC_TRIP    DL_XBAR_TRIP2
#define OC_SIGNAL  DL_MCPWM_TZ_SIGNAL_OST2

#ifdef AM13E_E62_POWER_STAGE_PROFILE
#if AM13E_E62_POWER_STAGE_PROFILE != 1
#error "AM13E_E62_POWER_STAGE_PROFILE must equal 1"
#endif
#ifndef AM13E_MOTOR_BOARD_DEADBAND_VERIFIED
#error "Qualified power stage requires hardware RED/FED and polarity"
#endif
#if !defined(AM13E_E62_PB13_ACTIVE_LEVEL) || \
    !defined(AM13E_E62_GATE_PWM_INVERT_MASK) || \
    !defined(AM13E_E62_OC_GPIO_PINCM) || \
    !defined(AM13E_E62_OC_ACTIVE_LOW) || \
    !defined(AM13E_E62_GATE_INPUTS_HIZ_SAFE) || \
    !defined(AM13E_E62_GATE_DRIVER_HAS_HW_SHUTDOWN)
#error "No guessed gate/OC configuration: all six board fields required"
#endif
_Static_assert((AM13E_E62_PB13_ACTIVE_LEVEL==0 ||
                AM13E_E62_PB13_ACTIVE_LEVEL==1) &&
               AM13E_E62_GATE_PWM_INVERT_MASK>=0 &&
               AM13E_E62_GATE_PWM_INVERT_MASK<=63 &&
               (AM13E_E62_OC_ACTIVE_LOW==0 ||
                AM13E_E62_OC_ACTIVE_LOW==1) &&
               AM13E_E62_GATE_INPUTS_HIZ_SAFE==1 &&
               AM13E_E62_GATE_DRIVER_HAS_HW_SHUTDOWN==1 &&
               AM13E_E62_OC_GPIO_PINCM>0 &&
               AM13E_E62_OC_GPIO_PINCM<64 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PB13 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PB14 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PB15 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA8 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA11 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA9 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA30 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA10 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA31 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA17 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA4 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA3 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA2 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA16 &&
               AM13E_E62_OC_GPIO_PINCM!=IOMUX_PINCM_PA18,
               "Unqualified/conflicting E62 power stage configuration");
#endif

static volatile uint32_t initialized;
static volatile uint32_t attached;

int am13e_power_stage_board_profile_present(void)
{
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    return 1;
#else
    return 0;
#endif
}

#ifdef AM13E_E62_POWER_STAGE_PROFILE
static void driver_enable_level(int active)
{
    const int high=active ? AM13E_E62_PB13_ACTIVE_LEVEL :
                            !AM13E_E62_PB13_ACTIVE_LEVEL;
    if(high) DL_GPIO_setPins(GPIO1,GATE_EN_PIN);
    else DL_GPIO_clearPins(GPIO1,GATE_EN_PIN);
}
static void output_pin(uint32_t pincm,uint32_t function,unsigned bit)
{
    const DL_GPIO_INVERSION inv=
       (AM13E_E62_GATE_PWM_INVERT_MASK&(1U<<bit)) ?
           DL_GPIO_INVERSION_ENABLE:DL_GPIO_INVERSION_DISABLE;
    DL_GPIO_initPeripheralOutputFunctionFeatures(pincm,function,inv,
        DL_GPIO_RESISTOR_NONE,DL_GPIO_DRIVE_STRENGTH_LOW,
        DL_GPIO_HIZ_DISABLE);
}
static int pwm_function_readback(void)
{
    return DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA8)==
                 IOMUX_PA8_MCPWM0_1A &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA11)==
                 IOMUX_PA11_MCPWM0_1B &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA9)==
                 IOMUX_PA9_MCPWM0_2A &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA30)==
                 IOMUX_PA30_MCPWM0_2B &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA10)==
                 IOMUX_PA10_MCPWM0_3A &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA31)==
                 IOMUX_PA31_MCPWM0_3B;
}
static int driver_level_matches(int active)
{
    const int high=active ? AM13E_E62_PB13_ACTIVE_LEVEL :
                            !AM13E_E62_PB13_ACTIVE_LEVEL;
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
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    const uint32_t selected=PWMXBAR->PWM_XBAR_GXSEL[1].PWMXBARG0SEL;
    const uint32_t tzflag=DL_MCPWM_getTripZoneFlagStatus(MCPWM0);
    const uint32_t inverted=PWMXBAR->PWMXBAROUTINVERT & (1U<<1U);
    return initialized &&
           INPUTXBAR->INPUTSELECT[2]==AM13E_E62_OC_GPIO_PINCM &&
           (selected & (1U<<(unsigned)OC_SOURCE))!=0U &&
           (inverted!=0U)==(AM13E_E62_OC_ACTIVE_LOW!=0) &&
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
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    /* Set inactive PB13 data BEFORE enabling its digital output.
     * Never connect PWM pins until independent OST1 and OST2 exist.
     */
    DL_GPIO_enablePower(GPIO1);
    if(!DL_GPIO_isPowerEnabled(GPIO1)) for(;;){__NOP();}
    driver_enable_level(0);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB13);
    DL_GPIO_enableOutput(GPIO1,GATE_EN_PIN);
    DL_GPIO_initDigitalInput(AM13E_E62_OC_GPIO_PINCM);
    DL_XBAR_enableRawInput(AM13E_E62_OC_GPIO_PINCM);
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT3,AM13E_E62_OC_GPIO_PINCM);
    DL_XBAR_clearPWMXBARSourceSelection(OC_TRIP);
    DL_XBAR_selectPWMXBARSource(OC_TRIP,OC_SOURCE);
    DL_XBAR_invertPWMXBARSignal(OC_TRIP,AM13E_E62_OC_ACTIVE_LOW!=0);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZA,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_setTripZoneAction(MCPWM0,DL_MCPWM_TZ_ACTION_EVENT_TZB,
                               DL_MCPWM_TZ_ACTION_HIGH_Z);
    DL_MCPWM_enableTripZoneSignals(MCPWM0,OC_SIGNAL);
#endif
    attached=0U;
    initialized=1U;
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    if (!driver_level_matches(0) ||
        !am13e_power_stage_oc_trip_ready() ||
        !am13e_app_motor_nfault_trip_ready())
        for(;;){__NOP();}
#endif
}

void am13e_power_stage_force_off(void)
{
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    if(initialized) {
        driver_enable_level(0);
        __DSB();
        set_pwm_force(DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
        DL_GPIO_disableOutput(GPIO0,PWM_PADS);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA8);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA11);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA9);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA30);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA10);
        DL_GPIO_initDigitalInput(IOMUX_PINCM_PA31);
    }
#endif
    attached=0U;
}

int am13e_power_stage_attached(void)
{
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    return initialized && attached && driver_level_matches(1) &&
           (GPIO0->DOE31_0&PWM_PADS)==0U &&
           pwm_function_readback() && !am13e_app_nfault_asserted() &&
           am13e_power_stage_oc_trip_ready() &&
           am13e_app_motor_nfault_trip_ready();
#else
    return 0;
#endif
}

int am13e_power_stage_attach(void)
{
#ifdef AM13E_E62_POWER_STAGE_PROFILE
    if (!initialized || attached ||
        __get_PRIMASK()!=1U || !driver_level_matches(0) ||
        !am13e_power_stage_oc_trip_ready() ||
        !am13e_app_motor_nfault_trip_ready() ||
        am13e_app_nfault_asserted() ||
        (MCPWM0->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)!=
             (uint32_t)DL_MCPWM_COUNTER_MODE_UP ||
        (MCPWM0->PWM1_AQSFRC & 0x33U)!=0x11U ||
        (MCPWM0->PWM2_AQSFRC & 0x33U)!=0x11U ||
        (MCPWM0->PWM3_AQSFRC & 0x33U)!=0x11U)
        return 0;
    /* External driver still disabled; only now change all six IOMUXes. */
    output_pin(IOMUX_PINCM_PA8,IOMUX_PA8_MCPWM0_1A,0U);
    output_pin(IOMUX_PINCM_PA11,IOMUX_PA11_MCPWM0_1B,1U);
    output_pin(IOMUX_PINCM_PA9,IOMUX_PA9_MCPWM0_2A,2U);
    output_pin(IOMUX_PINCM_PA30,IOMUX_PA30_MCPWM0_2B,3U);
    output_pin(IOMUX_PINCM_PA10,IOMUX_PA10_MCPWM0_3A,4U);
    output_pin(IOMUX_PINCM_PA31,IOMUX_PA31_MCPWM0_3B,5U);
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
    return 0; /* Intentionally NOT a fake-success physical power stage. */
#endif
}
