#include "../config.h"
/* AM13E reference: PA22 nFAULT (GPIO22) -> INPUTXBAR2 -> TRIP2 -> MOTOR_PWM OST2.
 * This is an asynchronous driver-fault hardware interlock, distinct from
 * independent over-current sensing. Never touches PB13 or motor pad mux.
 * Software must not clear the latched OST while nFAULT is unqualified.
 */
#include <stdint.h>
#include <soc.h>
#include <dl_xbar.h>
#include <dl_mcpwm.h>
#include "motor_backend.h"
#include "motor_fault_route.h"
#include "fault_trip_backend.h"
#include "board_configuration.h"
#include <dl_gpio.h>

#define AM13E_REF_NFAULT_GPIO_INDEX AM13E_NFAULT_PINCM
#define AM13E_REF_NFAULT_INPUTXBAR AM13E_NFAULT_INPUTXBAR
#define AM13E_REF_NFAULT_PWMXBAR AM13E_NFAULT_PWMXBAR
#define AM13E_REF_NFAULT_TZ_SIGNAL AM13E_NFAULT_TZ_SIGNAL
#define AM13E_REF_NFAULT_SOURCE AM13E_NFAULT_SOURCE

_Static_assert(AM13E_REF_NFAULT_SOURCE == 0x15U &&
               AM13E_REF_NFAULT_TZ_SIGNAL == MCPWM_TZSEL_OST2_MASK &&
               IOMUX_PINCM_PA22 == 22U,
               "AM13E reference PA22 nFAULT hardware TZ routing changed");

/* Reference routing is deliberately owned by this board adapter. */
static const AM13E_FaultTripRoute reference_nfault_route = {
    .mcpwm = MOTOR_PWM,
    .gpio_index = AM13E_REF_NFAULT_GPIO_INDEX,
    .input_xbar = AM13E_REF_NFAULT_INPUTXBAR,
    .pwm_trip = AM13E_REF_NFAULT_PWMXBAR,
    .pwm_source = AM13E_REF_NFAULT_SOURCE,
    .ost_signal = AM13E_REF_NFAULT_TZ_SIGNAL,
    .ost_flag = DL_MCPWM_TZ_FLAG_OST_TZ2,
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    .active_low = AM13E_MOTOR_NFAULT_ACTIVE_LOW!=0
#else
    .active_low = true
#endif
};

static volatile uint32_t hardware_trip_installed;

int am13e_app_motor_nfault_trip_ready(void)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    if(!NVIC_GetEnableIRQ(MOTOR_PWM_IRQ) || NVIC_GetPriority(MOTOR_PWM_IRQ)!=0U ||
       !(MOTOR_PWM->INTEN&DL_MCPWM_INT_TZ_OST) || !DL_GPIO_isInputEnabled(IOMUX_PINCM_PA22) ||
       DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA22)!=IOMUX_PA22_GPIO22 ||
       (((AM13E_NFAULT_GPIO->DIN31_0&AM13E_NFAULT_MASK)!=0U)==(AM13E_MOTOR_NFAULT_ACTIVE_LOW==0)))return 0;
#endif
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    if(!am13e_app_motor_oc_trip_ready())return 0;
#endif
    return hardware_trip_installed &&
           am13e_mcu_fault_trip_ready(&reference_nfault_route);
}

void am13e_app_motor_nfault_trip_init(void)
{
    if (__get_PRIMASK() == 0U || hardware_trip_installed)
        am13e_app_motor_fault_reset();
    /* A low nFAULT is allowed to latch OST, not cleared. Boot and
     * AM13E_NFAULT_GPIO/PA22 ownership, motor gate fail-closed policy unchanged.
     */
    am13e_mcu_fault_trip_install(&reference_nfault_route);
    hardware_trip_installed = 1U;
    if (!am13e_app_motor_nfault_trip_ready())
        am13e_app_motor_fault_reset();
}

/* Native asynchronous phase-OC route. Raw DACH codes and amplifier/output
 * polarity are explicit board contracts, not invented current thresholds.
 * Missing contracts cannot supply the required asynchronous protection. */
#include <dl_cmpss_lite.h>
#include <dl_sysctl.h>
#if defined(AM13E_OC_U_DAC_CODE) || defined(AM13E_OC_V_DAC_CODE) || \
    defined(AM13E_OC_W_DAC_CODE) || defined(AM13E_OC_U_INVERT) || \
    defined(AM13E_OC_V_INVERT) || defined(AM13E_OC_W_INVERT)
#if !defined(AM13E_OC_U_DAC_CODE) || !defined(AM13E_OC_V_DAC_CODE) || \
    !defined(AM13E_OC_W_DAC_CODE) || !defined(AM13E_OC_U_INVERT) || \
    !defined(AM13E_OC_V_INVERT) || !defined(AM13E_OC_W_INVERT)
#error "Option 1 OC requires all three explicit DACH codes and polarities"
#endif
#define OC_CONFIGURED 1
_Static_assert(AM13E_OC_U_DAC_CODE>0 && AM13E_OC_U_DAC_CODE<=4095 &&
               AM13E_OC_V_DAC_CODE>0 && AM13E_OC_V_DAC_CODE<=4095 &&
               AM13E_OC_W_DAC_CODE>0 && AM13E_OC_W_DAC_CODE<=4095 &&
               (AM13E_OC_U_INVERT==0 || AM13E_OC_U_INVERT==1) &&
               (AM13E_OC_V_INVERT==0 || AM13E_OC_V_INVERT==1) &&
               (AM13E_OC_W_INVERT==0 || AM13E_OC_W_INVERT==1),
               "Invalid Option 1 OC DACH code or polarity");
#else
#define OC_CONFIGURED 0
#endif
#if AM13E_MOTOR_OUTPUT_CONFIGURED && !OC_CONFIGURED
#error "Configured Option 1 outputs require explicit overcurrent contracts"
#endif
_Static_assert(DL_XBAR_PWM_CMPSS1_CTRIPH==2U && DL_XBAR_PWM_CMPSS2_CTRIPH==4U &&
               DL_XBAR_PWM_CMPSS3_CTRIPH==6U && DL_XBAR_TRIP3==2U &&
               DL_MCPWM_TZ_SIGNAL_OST3==0x40000U && DL_MCPWM_TZ_FLAG_OST_TZ3==0x40000U,
               "Option 1 OC XBAR/OST3 encoding drift");
static uint32_t oc_installed;
#if OC_CONFIGURED
static void oc_comparator(CMPSS_LITE_Regs *cmp,DL_SYSCTL_PERIPH_POWER power,
                          DL_SYSCTL_CMPSS_MUX mux,DL_SYSCTL_CMP_HP hp,
                          unsigned pin,unsigned code,unsigned invert)
{
    DL_SYSCTL_enablePower(power);
    if(!DL_SYSCTL_isPowerEnabled(power))am13e_app_motor_fault_reset();
    DL_CMPSSLITE_disableModule(cmp);
    DL_GPIO_initPeripheralAnalogFunction(pin);
    DL_SYSCTL_setCompartorHPMux(mux,hp);
    DL_CMPSSLITE_configHighComparator(cmp,DL_CMPSSLITE_INSRC_DAC |
        (invert?DL_CMPSSLITE_CONFIG_INVERT:0U));
    DL_CMPSSLITE_configDACHigh(cmp,DL_CMPSSLITE_DACVAL_MCLK);
    DL_CMPSSLITE_setDACValueLow(cmp,0U); /* TRM unused low-DAC constraint. */
    DL_CMPSSLITE_setDACValueHigh(cmp,(uint16_t)code);
    DL_CMPSSLITE_configOutputsHigh(cmp,DL_CMPSSLITE_TRIP_ASYNC_COMP);
    DL_CMPSSLITE_enableModule(cmp);
}
#endif
void am13e_app_motor_oc_trip_init(void)
{
    if(__get_PRIMASK()!=1U || oc_installed)am13e_app_motor_fault_reset();
#if OC_CONFIGURED
    oc_comparator(CMPSS1,DL_SYSCTL_PWREN_CMPSS1,DL_SYSCTL_CMPSS1_MUX,
                  DL_SYSCTL_CMP_HP1,AM13E_CURRENT_W_PINCM,AM13E_OC_W_DAC_CODE,AM13E_OC_W_INVERT);
    oc_comparator(CMPSS2,DL_SYSCTL_PWREN_CMPSS2,DL_SYSCTL_CMPSS2_MUX,
                  DL_SYSCTL_CMP_HP3,AM13E_CURRENT_V_PINCM,AM13E_OC_V_DAC_CODE,AM13E_OC_V_INVERT);
    oc_comparator(CMPSS3,DL_SYSCTL_PWREN_CMPSS3,DL_SYSCTL_CMPSS3_MUX,
                  DL_SYSCTL_CMP_HP1,AM13E_CURRENT_U_PINCM,AM13E_OC_U_DAC_CODE,AM13E_OC_U_INVERT);
    DL_XBAR_clearPWMXBARSourceSelection(DL_XBAR_TRIP3);
    DL_XBAR_selectPWMXBARSource(DL_XBAR_TRIP3,DL_XBAR_PWM_CMPSS1_CTRIPH);
    DL_XBAR_selectPWMXBARSource(DL_XBAR_TRIP3,DL_XBAR_PWM_CMPSS2_CTRIPH);
    DL_XBAR_selectPWMXBARSource(DL_XBAR_TRIP3,DL_XBAR_PWM_CMPSS3_CTRIPH);
    DL_XBAR_invertPWMXBARSignal(DL_XBAR_TRIP3,false);
    DL_MCPWM_enableTripZoneSignals(MOTOR_PWM,DL_MCPWM_TZ_SIGNAL_OST3);
    oc_installed=1U;
#endif
}
int am13e_app_motor_oc_trip_ready(void)
{
    return oc_installed &&
        PWMXBAR->PWM_XBAR_GXSEL[DL_XBAR_TRIP3].PWMXBARG0SEL==0x54U &&
        PWMXBAR->PWM_XBAR_GXSEL[DL_XBAR_TRIP3].PWMXBARG1SEL==0U &&
        !(PWMXBAR->PWMXBAROUTINVERT&(1U<<DL_XBAR_TRIP3)) &&
        (MOTOR_PWM->TZSEL&DL_MCPWM_TZ_SIGNAL_OST3) &&
        !(DL_MCPWM_getTripZoneFlagStatus(MOTOR_PWM)&DL_MCPWM_TZ_FLAG_OST_TZ3);
}
