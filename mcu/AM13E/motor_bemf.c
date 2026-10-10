/* ESCape32 Rel17 BEMF: CMPSS0/1/3 CTRIPH -> ECAP1 -> Rel17 policy.
 * ECAP0 belongs to PB14 DShot; TIMG12 owns delayed commutation.
 * AM13E230x TRM SPRUJF2B Table 24-1: CTRIPH ECAP index 45/46/48.
 * External analog phase/neutral connections MUST be board-confirmed.
 */
#include "motor_backend.h"
#include "clock_backend.h"
#include "irq_vectors.h"
#include <soc.h>
#include <dl_ecap.h>
#include <dl_cmpss_lite.h>
#include <dl_sysctl.h>
#include <dl_gpio.h>
#include <stdint.h>

#define BEMF_ECAP ECAP1
#define BEMF_CLOCK_HZ (AM13E_APP_MCLK_HZ / 2U)
#define BEMF_FLAGS (DL_ECAP_ISR_SOURCE_CEVT1 | DL_ECAP_ISR_SOURCE_CEVT2 | \
                    DL_ECAP_ISR_SOURCE_CEVT3 | DL_ECAP_ISR_SOURCE_CEVT4 | \
                    DL_ECAP_ISR_SOURCE_CTROVF)
_Static_assert(DL_ECAP_INPUT_CMPSS0_CTRIPH == 45U &&
               DL_ECAP_INPUT_CMPSS1_CTRIPH == 46U &&
               DL_ECAP_INPUT_CMPSS3_CTRIPH == 48U,
               "TRM CMPSS/ECAP1 routing changed");

/* Product must explicitly supply six verified analog pins and mux values.
 * This does not waive independent overcurrent trip and gate-off checks.
 */
#ifdef AM13E_BEMF_BOARD_ANALOG_VERIFIED
#if !defined(AM13E_BEMF_CMP0_HP_PINCM) || !defined(AM13E_BEMF_CMP0_HN_PINCM) || \
    !defined(AM13E_BEMF_CMP1_HP_PINCM) || !defined(AM13E_BEMF_CMP1_HN_PINCM) || \
    !defined(AM13E_BEMF_CMP3_HP_PINCM) || !defined(AM13E_BEMF_CMP3_HN_PINCM) || \
    !defined(AM13E_BEMF_CMP0_HP_MUX) || !defined(AM13E_BEMF_CMP0_HN_MUX) || \
    !defined(AM13E_BEMF_CMP1_HP_MUX) || !defined(AM13E_BEMF_CMP1_HN_MUX) || \
    !defined(AM13E_BEMF_CMP3_HP_MUX) || !defined(AM13E_BEMF_CMP3_HN_MUX)
#error "BEMF requires schematic-verified CMPSS0/1/3 analog pin/mux assignments"
#endif
#endif

static volatile uint32_t initialized,selected_code,armed;
static volatile uint32_t captured_events,interval_us;
static void bemf_fault(void)
{
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
    for(;;) __NOP();
}
static void capture_stop(void)
{
    armed=0U;
    DL_ECAP_disableInterrupt(BEMF_ECAP,BEMF_FLAGS);
    DL_ECAP_stopCounter(BEMF_ECAP);
    DL_ECAP_clearInterrupt(BEMF_ECAP,BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
}
/* This cancellation is safe even if a fault occurs before BEMF init. */
void am13e_app_motor_bemf_abort(void)
{
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if(initialized)capture_stop();
    selected_code=0U;
    interval_us=0U;
    __set_PRIMASK(primask);
}

#ifdef AM13E_BEMF_BOARD_ANALOG_VERIFIED
static CMPSS_LITE_Regs *phase_cmp(unsigned phase)
{
    switch(phase) {
        case 1U:return CMPSS0;
        case 2U:return CMPSS1;
        case 3U:return CMPSS3;
        default:bemf_fault();
    }
    __builtin_unreachable();
}
static void configure_phase_cmp(CMPSS_LITE_Regs *cmp,
          DL_SYSCTL_PERIPH_POWER power,DL_SYSCTL_CMPSS_MUX id,
          uint32_t hp_pin,uint32_t hn_pin,
          DL_SYSCTL_CMP_HP hp,DL_SYSCTL_CMP_HN hn)
{
    DL_SYSCTL_enablePower(power);
    if(!DL_SYSCTL_isPowerEnabled(power))bemf_fault();
    DL_GPIO_initPeripheralAnalogFunction(hp_pin);
    DL_GPIO_initPeripheralAnalogFunction(hn_pin);
    DL_SYSCTL_setCompartorHPMux(id,hp);
    DL_SYSCTL_setCompartorHNMux(id,hn);
    DL_CMPSSLITE_configHighComparator(cmp,DL_CMPSSLITE_INSRC_PIN);
    DL_CMPSSLITE_configFilterInputHigh(cmp,DL_CMPSSLITE_FILTIN_COMPOUT);
    DL_CMPSSLITE_configFilterHigh(cmp,0U,5U,3U);
    DL_CMPSSLITE_initFilterHigh(cmp);
    DL_CMPSSLITE_configOutputsHigh(cmp,
         DL_CMPSSLITE_TRIP_FILTER|DL_CMPSSLITE_TRIPOUT_FILTER);
    DL_CMPSSLITE_enableModule(cmp);
}
#endif
void am13e_app_motor_bemf_init(void)
{
    if(__get_PRIMASK()==0U || initialized)bemf_fault();
    DL_SYSCTL_enablePower(DL_SYSCTL_PWREN_ECAP1);
    if(!DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_ECAP1))bemf_fault();
    DL_ECAP_Config config;
    DL_ECAP_initParamsSetDefault(&config);
    config.captureModeConfig.input=DL_ECAP_INPUT_CMPSS0_CTRIPH;
    config.captureModeConfig.prescalerValue=0U;
    config.captureModeConfig.continouousOrOneShot=DL_ECAP_ONE_SHOT_CAPTURE_MODE;
    config.captureModeConfig.wrapOrStopAtEvent=DL_ECAP_EVENT_1;
    config.captureModeConfig.captureEvent1Polarity=DL_ECAP_EVENT_RISING_EDGE;
    config.captureModeConfig.resetCounter=true;
    config.captureModeConfig.reArm=true;
    config.interruptsConfig.interruptSourceEnableMask=0U;
    DL_ECAP_init(BEMF_ECAP,&config);
    DL_ECAP_enableTimeStampCapture(BEMF_ECAP);
    capture_stop();
#ifdef AM13E_BEMF_BOARD_ANALOG_VERIFIED
    configure_phase_cmp(CMPSS0,DL_SYSCTL_PWREN_CMPSS0,DL_SYSCTL_CMPSS0_MUX,
        AM13E_BEMF_CMP0_HP_PINCM,AM13E_BEMF_CMP0_HN_PINCM,
        (DL_SYSCTL_CMP_HP)AM13E_BEMF_CMP0_HP_MUX,
        (DL_SYSCTL_CMP_HN)AM13E_BEMF_CMP0_HN_MUX);
    configure_phase_cmp(CMPSS1,DL_SYSCTL_PWREN_CMPSS1,DL_SYSCTL_CMPSS1_MUX,
        AM13E_BEMF_CMP1_HP_PINCM,AM13E_BEMF_CMP1_HN_PINCM,
        (DL_SYSCTL_CMP_HP)AM13E_BEMF_CMP1_HP_MUX,
        (DL_SYSCTL_CMP_HN)AM13E_BEMF_CMP1_HN_MUX);
    configure_phase_cmp(CMPSS3,DL_SYSCTL_PWREN_CMPSS3,DL_SYSCTL_CMPSS3_MUX,
        AM13E_BEMF_CMP3_HP_PINCM,AM13E_BEMF_CMP3_HN_PINCM,
        (DL_SYSCTL_CMP_HP)AM13E_BEMF_CMP3_HP_MUX,
        (DL_SYSCTL_CMP_HN)AM13E_BEMF_CMP3_HN_MUX);
#endif
    initialized=1U;
    NVIC_SetPriority(ECAP1_INT_IRQn,0U);
    NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
    NVIC_EnableIRQ(ECAP1_INT_IRQn);
}
/* Rel17 x&3 selects the previous BEMF phase; x&4 reverses polarity.
 * Keep source change isolated from the interval filter/IRQ arming.
 */
void compctl(int x)
{
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if(!initialized || x<0 || x>7)bemf_fault();
    capture_stop();
    selected_code=0U;
    if((x&3)!=0) {
#ifndef AM13E_BEMF_BOARD_ANALOG_VERIFIED
        /* No invented analog net or comparator validity. */
        bemf_fault();
#else
        const unsigned phase=(unsigned)x&3U;
        const DL_ECAP_INPUT input=phase==1U?
            DL_ECAP_INPUT_CMPSS0_CTRIPH:phase==2U?
            DL_ECAP_INPUT_CMPSS1_CTRIPH:DL_ECAP_INPUT_CMPSS3_CTRIPH;
        DL_ECAP_selectECAPInput(BEMF_ECAP,input);
        DL_ECAP_setEventPolarity(BEMF_ECAP,DL_ECAP_EVENT_1,
             (x&4)?DL_ECAP_EVENT_FALLING_EDGE:DL_ECAP_EVENT_RISING_EDGE);
        selected_code=(uint32_t)x;
#endif
    }
    __set_PRIMASK(primask);
}
/* Five original ertm bands preserved; CMPSS majority filter is not
 * STM32 TIM2 ICF-identical. Silicon latency characterization remains.
 */
void am13e_app_motor_bemf_interval_select(int ertm_us)
{
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if(!initialized || ertm_us<=0)bemf_fault();
    capture_stop();
    interval_us=(uint32_t)ertm_us;
    if(selected_code) {
#ifdef AM13E_BEMF_BOARD_ANALOG_VERIFIED
        const uint32_t prescale=ertm_us<100?0U:ertm_us<200?1U:
              ertm_us<1000?3U:ertm_us<2000?7U:15U;
        CMPSS_LITE_Regs *cmp=phase_cmp(selected_code&3U);
        DL_CMPSSLITE_configFilterHigh(cmp,prescale,5U,3U);
        DL_CMPSSLITE_initFilterHigh(cmp);
        DL_ECAP_resetCounters(BEMF_ECAP);
        DL_ECAP_clearInterrupt(BEMF_ECAP,BEMF_FLAGS);
        DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
        NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
        DL_ECAP_enableInterrupt(BEMF_ECAP,
             DL_ECAP_ISR_SOURCE_CEVT1|DL_ECAP_ISR_SOURCE_CTROVF);
        armed=1U;
        DL_ECAP_startCounter(BEMF_ECAP);
#else
        bemf_fault();
#endif
    }
    __set_PRIMASK(primask);
}
void am13e_app_motor_bemf_stop(void)
{
    if(!initialized)bemf_fault();
    am13e_app_motor_bemf_abort();
}
/* Actual hardware edge -> timestamp -> original Rel17 desync/advance
 * logic. Hardware source is acknowledged before invoking the callback.
 */
void ECAP1_IRQHandler(void)
{
    const uint16_t flags=DL_ECAP_getInterruptSource(BEMF_ECAP);
    const uint32_t ticks=DL_ECAP_getEventTimeStamp(BEMF_ECAP,DL_ECAP_EVENT_1);
    DL_ECAP_clearInterrupt(BEMF_ECAP,flags&BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    if(!initialized)bemf_fault();
    if(!armed)return;
    armed=0U;
    DL_ECAP_disableInterrupt(BEMF_ECAP,BEMF_FLAGS);
    DL_ECAP_stopCounter(BEMF_ECAP);
    if(flags&DL_ECAP_ISR_SOURCE_CTROVF) {
        am13e_app_motor_on_bemf_event(0,1);
        return;
    }
    if((flags&DL_ECAP_ISR_SOURCE_CEVT1)==0U ||
       (flags&~BEMF_FLAGS)!=0U)bemf_fault();
    const uint64_t us=((uint64_t)ticks*UINT64_C(1000000)+
          BEMF_CLOCK_HZ/2U)/BEMF_CLOCK_HZ;
    if(us==0U || us>INT32_MAX)bemf_fault();
    ++captured_events;
    am13e_app_motor_on_bemf_event((int)us,0);
}
