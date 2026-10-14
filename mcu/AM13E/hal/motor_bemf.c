/* ESCape32 Rel17 BEMF: CMPSS0 H CTRIPH -> ECAP1 -> Rel17 policy.
 * TIMG4 belongs to PA26 DShot; TIMG12 owns delayed commutation.
 * AM13E230x TRM SPRUJF2B Table 24-1: CMPSS0 CTRIPH ECAP index 45.
 * External analog phase/neutral connections MUST be board-confirmed.
 */
#include "motor_backend.h"
#include "motor_audio_hw.h" /* Prevent COMP/ECAP1 rearming during sound */
#include "motor_bemf.h" /* Declarations for init, abort and ECAP1 IRQ */
#include "motor_bemf_event_plan.h"
#include "board_reference_io.h" /* Provisional AM13E reference phase/BEMF pinmux */
#include "motor_event_timer.h" /* Cancel obsolete TIMG12 on BEMF timeout */
#include "clock_backend.h"
#include "../config.h"
#include "irq_vectors.h"
#include <soc.h>
#include <dl_ecap.h>
#include <dl_mcpwm.h>
#include "motor_output_runtime.h"
#ifdef SW_BLANKING
static volatile uint32_t blank_required,blank_active;
#define BLANK_EVENTS (DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2)
#endif
#include <dl_cmpss_lite.h>
#include <dl_sysctl.h>
#include <dl_gpio.h>
#include <stdint.h>
#ifdef AM13E_BOARD_SC5_CALIBRATION
#include "analog_runtime.h"
extern volatile uint32_t tick;
static AM13E_BemfReference staged_reference;
static uint16_t expected_dach;
static uint32_t reference_calculated,reference_published;
#ifdef AM13E_BEMF_INITIAL_DAC_CODE
#error "Dynamic SC5 requires an acquired calibrated sample, not a raw initial DACH seed"
#endif
#endif

#define BEMF_ECAP BEMF_CAPTURE
/* ECAP1 clock can differ from MCPWM/TIMG12 BUSCLK. It is measured
 * against the verified 16 kHz SysTick before any BEMF capture arms.
 */
/* Rel17 STM32G431: CLK=168MHz, IFTIM_XRES=2, TIM2 PSC=20
 * -> 8MHz TIM2. src/main.c sets IFTIM ARR to
 * (1 << (IFTIM_XRES + 16)) - 1 = 262143, NOT 65535.
 * Thus UPDATE timeout = 262144 / 8MHz = 32768us. The previous
 * 8192us adaptation incorrectly shortened the Rel17 BEMF window 4x.
 * ECAP1 32-bit overflow (~42.9s at 100MHz) cannot substitute for it.
 * SysTick supervision resolution is nominal 62.5us at 16kHz.
 */
#define BEMF_TIMEOUT_US AM13E_BEMF_REL17_TIMEOUT_US
#define BEMF_CALIB_SYSTICKS 16U /* 1ms at 16kHz */
#define BEMF_FLAGS (DL_ECAP_ISR_SOURCE_CEVT1 | DL_ECAP_ISR_SOURCE_CEVT2 | \
                    DL_ECAP_ISR_SOURCE_CEVT3 | DL_ECAP_ISR_SOURCE_CEVT4 | \
                    DL_ECAP_ISR_SOURCE_CTROVF)
_Static_assert(DL_ECAP_INPUT_CMPSS0_CTRIPH == 45U,
               "TRM CMPSS0/ECAP1 route changed");

/* Retain the original public Rel17 comparator-control signature without
 * importing legacy STM32 GPIO/timer implementation into this backend.
 */
void compctl(int x);

static volatile uint32_t initialized,selected_code,armed,reference_ready;
static volatile uint32_t pending_code,pending_interval;
/* Native free-running timestamp epoch. Only an accepted crossing advances
 * this reference; commutation, blanking and rejected edges do not. */
static uint32_t crossing_epoch,crossing_epoch_valid;
static volatile uint32_t captured_events,rejected_events,interval_us;
/* 16 complete SysTick intervals form 1ms; retain the FULL ECAP tick
 * count instead of rounding to an integer MHz (ticks/us).
 */
static volatile uint32_t capture_ticks_per_ms, capture_timeout_ticks;
static volatile uint32_t calibration_ticks,calibration_start;
static uint32_t calibration_start_after,calibration_phase;
static volatile uint32_t calibration_done;
static volatile uint32_t calibration_halted;
static void bemf_fault(void)
{
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
    for(;;) __NOP();
}
/* Only BEMF-owned CMPSS0. A board-qualified OC trip must use
 * independently mapped protection hardware, not these sense outputs.
 * Match Rel17 compctl(0) which disables the sensing comparators.
 */
#ifdef AM13E_REF_IO_PLAN_V1
static void sense_comparators_off(void)
{
    DL_CMPSSLITE_disableModule(CMPSS0);
}
#endif

static void capture_stop(void)
{
    armed=0U;
#ifdef SW_BLANKING
    blank_required=blank_active=0U;
    uint32_t events=DL_MCPWM_INT_ET_2;
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    if(!am13e_motor_output_start_pending())
#endif
        events|=DL_MCPWM_INT_ET_1;
    DL_MCPWM_disableInterrupt(MOTOR_PWM,events);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,events);
    DL_ECAP_disableTimeStampCapture(BEMF_ECAP);
#endif
    DL_ECAP_disableTimeStampCapture(BEMF_ECAP);
    DL_ECAP_disableInterrupt(BEMF_ECAP,BEMF_FLAGS);
    if(!crossing_epoch_valid) {
        DL_ECAP_stopCounter(BEMF_ECAP);
        if(!calibration_done)calibration_ticks=0U;
    }
    DL_ECAP_clearInterrupt(BEMF_ECAP,BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
}
/* This cancellation is safe even if a fault occurs before BEMF init. */
void am13e_app_motor_bemf_abort(void)
{
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    pending_code=pending_interval=0U;
#ifdef AM13E_BOARD_SC5_CALIBRATION
    reference_calculated=reference_published=reference_ready=0U;
#endif
    if(initialized) {
        capture_stop();
        DL_ECAP_stopCounter(BEMF_ECAP);
#ifdef AM13E_REF_IO_PLAN_V1
        sense_comparators_off();
#endif
    }
    selected_code=0U;
    crossing_epoch_valid=0U;
    interval_us=0U;
    if(initialized) {
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        if(!am13e_motor_output_start_pending())
#endif
            DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    }
    __set_PRIMASK(primask);
}
void am13e_app_motor_bemf_fault_stop(void)
{
    /* Never preserve/restart ECAP timing through a latched motor fault. */
    __disable_irq();
    calibration_halted=1U;
    am13e_app_motor_bemf_abort();
}

#ifdef AM13E_REF_IO_PLAN_V1
static DL_SYSCTL_CMP_HP phase_hp(unsigned logical)
{
    switch(logical){
        case 1U:return DL_SYSCTL_CMP_HP0;
        case 2U:return DL_SYSCTL_CMP_HP1;
        case 3U:return DL_SYSCTL_CMP_HP2;
        default:bemf_fault();
    }
    __builtin_unreachable();
}
static void configure_bemf_cmp(void)
{
    DL_SYSCTL_enablePower(DL_SYSCTL_PWREN_CMPSS0);
    if(!DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_CMPSS0))bemf_fault();
    DL_CMPSSLITE_disableModule(CMPSS0);
    DL_GPIO_initPeripheralAnalogFunction(AM13E_BEMF_U_PINCM);
    DL_GPIO_initPeripheralAnalogFunction(AM13E_BEMF_V_PINCM);
    DL_GPIO_initPeripheralAnalogFunction(AM13E_BEMF_W_PINCM);
    DL_SYSCTL_setCompartorHPMux(DL_SYSCTL_CMPSS0_MUX,DL_SYSCTL_CMP_HP0);
    DL_CMPSSLITE_configHighComparator(CMPSS0,DL_CMPSSLITE_INSRC_DAC);
#ifdef AM13E_BOARD_SC5_CALIBRATION
    /* Motor initialization has already configured SYNCPER and a nonzero
     * period. TRM22.5.1: do not configure a level load before the PWM. */
    DL_CMPSSLITE_configureSyncSourceHigh(CMPSS0,DL_CMPSSLITE_SYNCSOURCE_MCPWM4_SYNCPER);
    DL_CMPSSLITE_configDACHigh(CMPSS0,DL_CMPSSLITE_DACVAL_PWMSYNC);
#else
    DL_CMPSSLITE_configDACHigh(CMPSS0,DL_CMPSSLITE_DACVAL_MCLK);
#endif
    DL_CMPSSLITE_setDACValueLow(CMPSS0,0U); /* TRM: unused low DAC must not limit DACH. */
    DL_CMPSSLITE_setDACValueHigh(CMPSS0,0U);
    DL_CMPSSLITE_configFilterInputHigh(CMPSS0,DL_CMPSSLITE_FILTIN_COMPOUT);
    DL_CMPSSLITE_configFilterHigh(CMPSS0,0U,5U,3U);
    DL_CMPSSLITE_initFilterHigh(CMPSS0);
    DL_CMPSSLITE_configOutputsHigh(CMPSS0,
         DL_CMPSSLITE_TRIP_FILTER|DL_CMPSSLITE_TRIPOUT_FILTER);
    reference_ready=0U; /* Zero is an inactive value, not a Vdc/2 calibration. */
}
#endif
void am13e_app_motor_bemf_init(void)
{
    if(__get_PRIMASK()==0U || initialized || calibration_halted)bemf_fault();
    DL_SYSCTL_enablePower(DL_SYSCTL_PWREN_ECAP1);
    if(!DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_ECAP1))bemf_fault();
    DL_ECAP_Config config;
    DL_ECAP_initParamsSetDefault(&config);
    config.captureModeConfig.input=DL_ECAP_INPUT_CMPSS0_CTRIPH;
    config.captureModeConfig.prescalerValue=0U;
    config.captureModeConfig.continouousOrOneShot=DL_ECAP_CONTINUOUS_CAPTURE_MODE;
    config.captureModeConfig.wrapOrStopAtEvent=DL_ECAP_EVENT_1;
    config.captureModeConfig.captureEvent1Polarity=DL_ECAP_EVENT_RISING_EDGE;
    config.captureModeConfig.resetCounter=true; /* reset only at init */
    config.captureModeConfig.enableCounterResetOnCaptureEvent1=false;
    config.captureModeConfig.reArm=true;
    config.interruptsConfig.interruptSourceEnableMask=0U;
    DL_ECAP_init(BEMF_ECAP,&config);
    /* Continuous capture retains the accepted-crossing epoch. Do not let
     * CEVT1 reset TSCTR, including when the Core rejects an early edge.
     */
    if (BEMF_ECAP->ECCTL1 & ECAP_ECCTL1_CTRRST1_MASK) bemf_fault();
    DL_ECAP_enableTimeStampCapture(BEMF_ECAP);
    capture_stop();
    /* Calibrate on an unarmed input: TSCTR is free running, while ECAP1
     * capture interrupts remain disabled and no motor output is enabled.
     * This is genuine counter timing used later for every zero-cross.
     */
    calibration_ticks=0U;
    calibration_done=0U;
    DL_ECAP_resetCounters(BEMF_ECAP);
    DL_ECAP_startCounter(BEMF_ECAP);
    calibration_start=0U; /* first SysTick provides phase-aligned origin */
#ifdef AM13E_REF_IO_PLAN_V1
    configure_bemf_cmp();
#endif
    initialized=1U;
#ifdef AM13E_BEMF_INITIAL_DAC_CODE
    if(!am13e_app_motor_bemf_reference_set(AM13E_BEMF_INITIAL_DAC_CODE))bemf_fault();
#endif
    NVIC_SetPriority(ECAP1_INT_IRQn,0U);
    NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
    NVIC_EnableIRQ(ECAP1_INT_IRQn);
}
/* Board-calibrated raw DACH code only. Live tracking/settling has no
 * established contract, so updates cannot disturb an armed capture.
 * The selected Reference never supplies a guessed Vdc/2 code. */
int am13e_app_motor_bemf_reference_set(uint16_t code)
{
#ifdef AM13E_BOARD_SC5_CALIBRATION
    (void)code;return 0; /* Tracking never accepts unassociated raw seeds. */
#else
    if(!initialized || __get_PRIMASK()!=1U || armed || selected_code || pending_code ||
       code==0U || code>4095U)return 0;
    DL_CMPSSLITE_disableModule(CMPSS0);
    DL_CMPSSLITE_setDACValueHigh(CMPSS0,code);
    DL_CMPSSLITE_initFilterHigh(CMPSS0);
    DL_ECAP_clearInterrupt(BEMF_ECAP,BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
    reference_ready=1U;
    return 1;
#endif
}

#ifdef AM13E_BOARD_SC5_CALIBRATION
static int reference_route_matches(void);
#endif
int am13e_app_motor_bemf_reference_ready(void)
{
#ifdef AM13E_BOARD_SC5_CALIBRATION
    AM13E_BemfReference latest;
    return initialized && reference_published &&
           am13e_app_adc_get_reference(&latest) && reference_route_matches() &&
           am13e_app_adc_reference_fresh(&staged_reference) &&
           DL_CMPSSLITE_getDACValueHigh(CMPSS0)==expected_dach;
#else
    return initialized && reference_ready;
#endif
}
int am13e_app_motor_bemf_reference_start_ready(void)
{
#ifdef AM13E_BOARD_SC5_CALIBRATION
    /* Starting TBCLK with driver held inactive precedes the first DAC
     * sync. The second startup ZERO still verifies active publication. */
    if(!selected_code && !pending_code)return 1; /* Core brake/audio/coast does not use BEMF. */
    return initialized && reference_calculated &&
           am13e_app_adc_reference_fresh(&staged_reference);
#else
    return am13e_app_motor_bemf_reference_ready();
#endif
}
int am13e_app_motor_bemf_reference_gate_ready(void)
{
#ifdef AM13E_BOARD_SC5_CALIBRATION
    return (!selected_code && !pending_code) || am13e_app_motor_bemf_reference_ready();
#else
    return am13e_app_motor_bemf_reference_ready();
#endif
}
void am13e_app_motor_bemf_reference_status(AM13E_BemfReferenceStatus *out)
{
    if(!out)return;
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    *out=(AM13E_BemfReferenceStatus){0};
#ifdef AM13E_BOARD_SC5_CALIBRATION
    out->configured=initialized && reference_route_matches();
    out->calculated=reference_calculated && am13e_app_adc_reference_fresh(&staged_reference);
    out->published=am13e_app_motor_bemf_reference_ready();
    out->capture_synchronized=out->published && armed;
    out->expected_code=expected_dach;out->sample=staged_reference.sample;
    if(initialized)out->active_code=DL_CMPSSLITE_getDACValueHigh(CMPSS0);
#endif
    __set_PRIMASK(mask);
}
#ifdef AM13E_BOARD_SC5_CALIBRATION
static int reference_route_matches(void)
{
    return (CMPSS0->COMPDACHCTL &
           (CMPSS_LITE_COMPDACHCTL_RAMPSOURCE_MASK|CMPSS_LITE_COMPDACHCTL_SWLOADSEL_MASK))==
           (((unsigned)DL_CMPSSLITE_SYNCSOURCE_MCPWM4_SYNCPER<<CMPSS_LITE_COMPDACHCTL_RAMPSOURCE_OFS)|
            DL_CMPSSLITE_DACVAL_PWMSYNC) &&
           (MOTOR_PWM->TBCTL&MCPWM_TBCTL_SYNCPERSEL_MASK)==
           ((unsigned)DL_MCPWM_SYNCPERSEL_SOURCE_ZERO<<MCPWM_TBCTL_SYNCPERSEL_OFS);
}
#endif

/* Rel17 x&3 selects the previous BEMF phase; x&4 reverses polarity.
 * Keep source change isolated from the interval filter/IRQ arming.
 */
void compctl(int x)
{
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if(!initialized || x<0 || x>7 ||
        (x!=0 && am13e_app_motor_audio_mode())) bemf_fault();
    if(x==0) {
        pending_code=pending_interval=0U;
#ifdef AM13E_BOARD_SC5_CALIBRATION
        reference_calculated=reference_published=reference_ready=0U;
#endif
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        if(!am13e_motor_output_start_pending())
#endif
            DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    }
    capture_stop();
    selected_code=0U;
    if(x==0) {
        crossing_epoch_valid=0U;
        DL_ECAP_stopCounter(BEMF_ECAP);
    }
#ifdef AM13E_REF_IO_PLAN_V1
    sense_comparators_off();
#endif
    if((x&3)!=0) {
#ifndef AM13E_REF_IO_PLAN_V1
        /* No invented analog net or comparator validity. */
        bemf_fault();
#else
        const unsigned phase=(unsigned)x&3U;
        DL_SYSCTL_setCompartorHPMux(DL_SYSCTL_CMPSS0_MUX,phase_hp(phase));
        DL_CMPSSLITE_initFilterHigh(CMPSS0);
        const DL_ECAP_INPUT input=DL_ECAP_INPUT_CMPSS0_CTRIPH;
        DL_ECAP_selectECAPInput(BEMF_ECAP,input);
        if ((BEMF_ECAP->ECCTL0 & ECAP_ECCTL0_INPUTSEL_MASK) !=
                (uint32_t)input)
            bemf_fault();
        DL_ECAP_setEventPolarity(BEMF_ECAP,DL_ECAP_EVENT_1,
             /* G431 captures rising(reference > phase), optionally inverted.
              * Native CMPSS is phase > DACH: reverse that edge relation. */
             (x&4)?DL_ECAP_EVENT_RISING_EDGE:DL_ECAP_EVENT_FALLING_EDGE);
        if(reference_ready)DL_CMPSSLITE_enableModule(CMPSS0);
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
    if(!initialized || (ertm_us>0 && am13e_app_motor_audio_mode()))
        bemf_fault();
    capture_stop();
    /* Rel17 laststep() with lock may call nextstep() at rest, with
     * ertm==0. Do not claim BEMF availability or manufacture a timeout
     * capture for a stationary motor. Real running starts with ertm>0.
     */
    if (ertm_us<=0) {
        pending_code=pending_interval=0U;
        interval_us=0U;
        __set_PRIMASK(primask);
        return;
    }
    interval_us=(uint32_t)ertm_us;
    if(pending_code) {
        pending_interval=(uint32_t)ertm_us;
        DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_1,DL_MCPWM_EVT_TBCTR_ZERO);
        DL_MCPWM_setEventTriggerEventPrescale(MOTOR_PWM,DL_MCPWM_ET_1,1U);
        DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
        DL_MCPWM_enableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
        __set_PRIMASK(primask);
        return;
    }
    rejected_events=0U;
#ifdef AM13E_BOARD_SC5_CALIBRATION
    /* Digital publication is not analog qualification. The selected
     * production Reference cannot rearm a newly tracked analog path. */
    if(!AM13E_OPTION1_OUTPUTS_QUALIFIED || !am13e_app_motor_bemf_reference_ready()){
        __set_PRIMASK(primask);return;
    }
#endif
    if(selected_code && reference_ready) {
        if (!calibration_done || capture_ticks_per_ms == 0U ||
            capture_timeout_ticks == 0U) bemf_fault();
#ifdef AM13E_REF_IO_PLAN_V1
        const uint32_t prescale=ertm_us<100?0U:ertm_us<200?1U:
              ertm_us<1000?3U:ertm_us<2000?7U:15U;
        CMPSS_LITE_Regs *cmp=CMPSS0;
        DL_CMPSSLITE_configFilterHigh(cmp,prescale,5U,3U);
        DL_CMPSSLITE_initFilterHigh(cmp);
        if(!crossing_epoch_valid) {
            crossing_epoch=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            crossing_epoch_valid=1U;
        }
        DL_ECAP_enableTimeStampCapture(BEMF_ECAP);
        DL_ECAP_clearInterrupt(BEMF_ECAP,BEMF_FLAGS);
        DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
        NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
        DL_ECAP_enableInterrupt(BEMF_ECAP,
             DL_ECAP_ISR_SOURCE_CEVT1|DL_ECAP_ISR_SOURCE_CTROVF);
        armed=1U;
#ifdef SW_BLANKING
        /* Original nextstep enables blanking only in its slowest band. */
        blank_required=(ertm_us>=2000);
        if(blank_required)DL_MCPWM_enableInterrupt(MOTOR_PWM,BLANK_EVENTS);
        am13e_app_motor_bemf_pwm_event();
#endif
        DL_ECAP_startCounter(BEMF_ECAP);
#else
        bemf_fault();
#endif
    }
    __set_PRIMASK(primask);
}
void am13e_app_motor_bemf_stage(unsigned code)
{
    if(__get_PRIMASK()!=1U || !initialized || code>7U || !(code&3U))bemf_fault();
    capture_stop();
    sense_comparators_off();
    selected_code=0U;
    pending_code=code;
    pending_interval=0U;
#ifdef AM13E_BOARD_SC5_CALIBRATION
    reference_calculated=reference_published=reference_ready=0U;
    if(!reference_route_matches())bemf_fault();
    if(am13e_app_adc_get_reference(&staged_reference)) {
        expected_dach=staged_reference.code[(code&3U)-1U];
        DL_CMPSSLITE_setDACValueHigh(CMPSS0,expected_dach);
        reference_calculated=1U;
    }
#endif
}
int am13e_app_motor_bemf_publication_pending(void) { return pending_code!=0U; }
int am13e_app_motor_bemf_carrier_zero(void)
{
    if(!pending_code || !pending_interval)return 0;
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    /* An old queued ZERO may precede the newly armed global-load latch.
     * Keep capture disarmed until hardware has consumed the entire image. */
    if(DL_MCPWM_getGlobalLoadOneShotLatchStatus(MOTOR_PWM))return 1;
#endif
#ifdef AM13E_BOARD_SC5_CALIBRATION
    if(!reference_route_matches())bemf_fault();
    if(!reference_calculated) {
        if(AM13E_OPTION1_OUTPUTS_QUALIFIED)bemf_fault();
        pending_code=pending_interval=0U;
        DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
        return 1;
    }
    if(!am13e_app_adc_reference_fresh(&staged_reference))bemf_fault();
    /* No busy wait and no assumption that the independent DAC load has
     * occurred merely because Global Load completed. Remain quarantined
     * until a subsequent native event, bounded by the explicit sample age. */
    if(DL_CMPSSLITE_getDACValueHigh(CMPSS0)!=expected_dach)return 1;
    reference_ready=reference_published=1U;
#endif
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    const unsigned code=pending_code,interval=pending_interval;
    pending_code=pending_interval=0U;
    compctl((int)code);
    am13e_app_motor_bemf_interval_select((int)interval);
    if(!am13e_app_motor_bemf_publication_pending()
#ifdef SW_BLANKING
       && !am13e_app_motor_bemf_blanking_enabled()
#endif
#if AM13E_MOTOR_OUTPUT_CONFIGURED
       && !am13e_motor_output_start_pending()
#endif
       )DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    __set_PRIMASK(mask);
    return 1;
}
void am13e_app_motor_bemf_stop(void)
{
    if(!initialized)bemf_fault();
    am13e_app_motor_bemf_abort();
}
#ifdef SW_BLANKING
int am13e_app_motor_bemf_blanking_enabled(void) { return armed && blank_required; }
void am13e_app_motor_bemf_pwm_event(void)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if(armed){
        /* Use actual position when ZERO/CMPC flags coalesce. Processing
         * a stale CMPC after a newer ZERO must not reopen the blank window. */
        const unsigned blank=blank_required &&
            DL_MCPWM_getTimeBaseCounterValue(MOTOR_PWM)<AM13E_BEMF_BLANK_TICKS;
        if(blank){
            /* Preserve an already latched crossing from before blanking.
             * Process it through the real ISR/Core policy, not a second
             * timing implementation. Never consume timestamp-less events
             * generated after CAPLDEN has been disabled. */
            if(!blank_active && (DL_ECAP_getInterruptSource(BEMF_ECAP)&DL_ECAP_ISR_SOURCE_CEVT1)){
                ECAP1_IRQHandler();
                if(!armed){__set_PRIMASK(mask);return;}
            }
            blank_active=1U;
            DL_ECAP_disableTimeStampCapture(BEMF_ECAP);
            DL_ECAP_disableInterrupt(BEMF_ECAP,DL_ECAP_ISR_SOURCE_CEVT1);
        }else{
            /* CAPLDEN suppresses timestamp loads, not capture event flags.
             * Discard those flags before reopening, without resetting the
             * elapsed counter or extending the original BEMF timeout. */
            if(blank_active){
                /* Re-enable loads before discarding stale flags: any flag
                 * raised after this clear has a fresh timestamp. The small
                 * reopening latency remains part of software blanking. */
                DL_ECAP_enableTimeStampCapture(BEMF_ECAP);
                DL_ECAP_clearInterrupt(BEMF_ECAP,DL_ECAP_ISR_SOURCE_CEVT1);
                if(DL_ECAP_getInterruptSource(BEMF_ECAP)&DL_ECAP_ISR_SOURCE_CTROVF){
                    ECAP1_IRQHandler();
                    if(!armed){__set_PRIMASK(mask);return;}
                }
                DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
                NVIC_ClearPendingIRQ(ECAP1_INT_IRQn);
            }
            blank_active=0U;
            DL_ECAP_enableTimeStampCapture(BEMF_ECAP);
            DL_ECAP_enableInterrupt(BEMF_ECAP,DL_ECAP_ISR_SOURCE_CEVT1);
        }
    }
    __set_PRIMASK(mask);
}
#endif
/* Actual hardware edge -> timestamp -> original Rel17 desync/advance
 * logic. Hardware source is acknowledged before invoking the callback.
 */
void ECAP1_IRQHandler(void)
{
    /* Do not touch an uninitialized/unpowered ECAP1 peripheral. */
    if(!initialized)bemf_fault();
    const uint16_t flags=DL_ECAP_getInterruptSource(BEMF_ECAP);
    DL_ECAP_clearInterrupt(BEMF_ECAP,flags&BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    /* compctl()/rearm may clear an already-dispatched old IRQ. Preserve
     * the newly armed capture when that vector has no peripheral event. */
    if(!armed || flags==0U)return;
    /* A native 32-bit wrap is not an elapsed timeout. Unsigned differences
     * remain valid across one wrap because the logical timeout is bounded
     * well below the counter range. A wrap-only vector has no CAP1 edge. */
    if(!crossing_epoch_valid)bemf_fault();
    if((flags&DL_ECAP_ISR_SOURCE_CTROVF) &&
       DL_ECAP_getTimeStampCounter(BEMF_ECAP)-crossing_epoch>=capture_timeout_ticks) {
        capture_stop();
        crossing_epoch_valid=0U;
        DL_ECAP_stopCounter(BEMF_ECAP);
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
        return;
    }
    if((flags&DL_ECAP_ISR_SOURCE_CEVT1)==0U &&
       flags==DL_ECAP_ISR_SOURCE_CTROVF)return;
#ifdef SW_BLANKING
    if(blank_active && !(flags&~DL_ECAP_ISR_SOURCE_CEVT1))return; /* Masked capture vector. */
#endif
    if((flags&DL_ECAP_ISR_SOURCE_CEVT1)==0U ||
       (flags&~BEMF_FLAGS)!=0U)bemf_fault();
    if (!calibration_done || !capture_ticks_per_ms) bemf_fault();
    const uint32_t ticks=DL_ECAP_getEventTimeStamp(BEMF_ECAP,DL_ECAP_EVENT_1);
    int capture_us=0;
    const AM13E_BemfSample sample=am13e_bemf_sample_plan(
        ticks-crossing_epoch,capture_ticks_per_ms,capture_timeout_ticks,&capture_us);
    if (sample==AM13E_BEMF_SAMPLE_TIMEOUT) {
        capture_stop();
        crossing_epoch_valid=0U;
        DL_ECAP_stopCounter(BEMF_ECAP);
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
        return;
    }
    if (sample!=AM13E_BEMF_SAMPLE_EDGE)bemf_fault();
    ++captured_events;
    /* Rel17 rejects crossings earlier than ival/2. Keep ECAP1
     * continuously capturing until the shared policy accepts an edge.
     * Capture counter is NOT reset on a rejected edge.
     */
    if(am13e_app_motor_on_bemf_event(capture_us,0)) {
        crossing_epoch=ticks;
        capture_stop();
    } else
        ++rejected_events;
}

/* 16kHz SysTick supervision. ECAP1 hardware provides the elapsed
 * time in actual ticks, so timeout does not depend on scheduler jitter.
 * A no-edge BEMF interval cannot wait for the 32-bit counter overflow.
 * Calling the unchanged Rel17 timeout policy preserves sync reset.
 */
void am13e_app_motor_bemf_tick(void)
{
    if (!initialized || calibration_halted) return;
#ifdef AM13E_BOARD_SC5_CALIBRATION
    if(reference_calculated && (selected_code || pending_code)) {
        AM13E_BemfReference latest;
        if(!am13e_app_adc_get_reference(&latest) ||
           !am13e_app_adc_reference_fresh(&staged_reference) || !reference_route_matches() ||
           (reference_published && DL_CMPSSLITE_getDACValueHigh(CMPSS0)!=expected_dach))bemf_fault();
    }
#endif
    /* Tick0..16 uses a real 1ms SysTick reference. No UART or MCU pin
     * is needed. Match the already-established PB14 ECAP0 approach.
     */
    if (!calibration_done) {
        /* Sampling from init() to the 16th tick would depend on the
         * initial fractional SysTick phase (up to 62.5us / 6.25%).
         * Take the start stamp ON the first SysTick and finish after
         * exactly sixteen complete subsequent 62.5us intervals.
         */
        if (calibration_ticks == 0U) {
            /* Normal reset/audio abort may stop the otherwise dedicated
             * ECAP timebase. Restart on a SysTick boundary and measure
             * sixteen NEW complete intervals, never the shortened window.
             * Audio owns MCPWM/TIMG12, not ECAP1. */
            DL_ECAP_startCounter(BEMF_ECAP);
            calibration_start=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            calibration_phase=SysTick->VAL;
            calibration_start_after=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            if(SysTick->LOAD+1U!=AM13E_APP_MCLK_HZ/AM13E_APP_SYSTICK_HZ)
                bemf_fault();
            calibration_ticks=1U;
            return;
        }
        if (++calibration_ticks == BEMF_CALIB_SYSTICKS+1U) {
            const uint32_t before=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            const uint32_t phase=SysTick->VAL;
            const uint32_t after=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            const uint32_t elapsed=AM13E_APP_MCLK_HZ/1000U;
            /* Complete 1ms reference under the selected 200MHz MCLK
             * contract; endpoint phase must not bias the conversion.
             */
            /* ECAP TSCTR and SysTick both count MCLK. Bracket each
             * SysTick phase read with ECAP reads: endpoint ISR latency
             * is a phase difference, not a changed timer frequency.
             * A missed interval, stopped counter or inconsistent clock
             * cannot satisfy this bracket. Restart a FULL window while
             * capture stays unqualified; never accept a shortened one.
             */
            const uint32_t expected=elapsed+calibration_phase-phase;
            if(phase>SysTick->LOAD || calibration_phase>SysTick->LOAD ||
               calibration_start_after-calibration_start>=SysTick->LOAD+1U ||
               after-before>=SysTick->LOAD+1U ||
               expected<before-calibration_start_after ||
               expected>after-calibration_start) {
                calibration_ticks=0U;
                return;
            }
            const uint64_t timeout=((uint64_t)BEMF_TIMEOUT_US*elapsed+
                                   UINT64_C(999))/UINT64_C(1000);
            if (timeout==0U || timeout>UINT32_MAX) bemf_fault();
            capture_ticks_per_ms=elapsed;
            capture_timeout_ticks=(uint32_t)timeout;
            calibration_done=1U;
        }
        return;
    }
    if (!armed) return;
    if (DL_ECAP_getTimeStampCounter(BEMF_ECAP)-crossing_epoch <
        capture_timeout_ticks) return;
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (armed && DL_ECAP_getTimeStampCounter(BEMF_ECAP)-crossing_epoch >=
                     capture_timeout_ticks) {
        capture_stop();
        crossing_epoch_valid=0U;
        DL_ECAP_stopCounter(BEMF_ECAP);
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
    }
    __set_PRIMASK(mask);
}
