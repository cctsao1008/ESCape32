/* ESCape32 Rel17 BEMF: CMPSS0/1/3 CTRIPH -> ECAP1 -> Rel17 policy.
 * ECAP0 belongs to PB14 DShot; TIMG12 owns delayed commutation.
 * AM13E230x TRM SPRUJF2B Table 24-1: CTRIPH ECAP index 45/46/48.
 * External analog phase/neutral connections MUST be board-confirmed.
 */
#include "motor_backend.h"
#include "motor_bemf.h" /* Declarations for init, abort and ECAP1 IRQ */
#include "board_io_plan_v1.h" /* Provisional E62 phase/BEMF pinmux */
#include "motor_event_timer.h" /* Cancel obsolete TIMG12 on BEMF timeout */
#include "clock_backend.h"
#include "irq_vectors.h"
#include <soc.h>
#include <dl_ecap.h>
#include <dl_cmpss_lite.h>
#include <dl_sysctl.h>
#include <dl_gpio.h>
#include <stdint.h>

#define BEMF_ECAP ECAP1
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
_Static_assert(DL_ECAP_INPUT_CMPSS0_CTRIPH == 45U &&
               DL_ECAP_INPUT_CMPSS1_CTRIPH == 46U &&
               DL_ECAP_INPUT_CMPSS3_CTRIPH == 48U,
               "TRM CMPSS/ECAP1 routing changed");

/* IO Plan v1.0 is proposed for TI review; it is not physical electrical
 * verification. It DOES specify usable analog pad / CMPSS mux candidates.
 * Gate outputs still remain isolated by motor_safety.c.
 */
_Static_assert((AM13E_BEMF_PHASE1_CMPSS_IDX == 0 ||
                AM13E_BEMF_PHASE1_CMPSS_IDX == 1 ||
                AM13E_BEMF_PHASE1_CMPSS_IDX == 3) &&
               (AM13E_BEMF_PHASE2_CMPSS_IDX == 0 ||
                AM13E_BEMF_PHASE2_CMPSS_IDX == 1 ||
                AM13E_BEMF_PHASE2_CMPSS_IDX == 3) &&
               (AM13E_BEMF_PHASE3_CMPSS_IDX == 0 ||
                AM13E_BEMF_PHASE3_CMPSS_IDX == 1 ||
                AM13E_BEMF_PHASE3_CMPSS_IDX == 3) &&
                AM13E_BEMF_PHASE1_CMPSS_IDX != AM13E_BEMF_PHASE2_CMPSS_IDX &&
                AM13E_BEMF_PHASE2_CMPSS_IDX != AM13E_BEMF_PHASE3_CMPSS_IDX &&
                AM13E_BEMF_PHASE1_CMPSS_IDX != AM13E_BEMF_PHASE3_CMPSS_IDX,
               "Rel17 logical BEMF phases require unique CMPSS0/1/3");

/* Retain the original public Rel17 comparator-control signature without
 * importing legacy STM32 GPIO/timer implementation into this backend.
 */
void compctl(int x);

static volatile uint32_t initialized,selected_code,armed;
static volatile uint32_t captured_events,rejected_events,interval_us;
/* 16 complete SysTick intervals form 1ms; retain the FULL ECAP tick
 * count instead of rounding to an integer MHz (ticks/us).
 */
static volatile uint32_t capture_ticks_per_ms, capture_timeout_ticks;
static volatile uint32_t calibration_ticks,calibration_start;
static volatile uint32_t calibration_done;
static void bemf_fault(void)
{
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
    for(;;) __NOP();
}
/* Only BEMF-owned CMPSS0/1/3. A board-qualified OC trip must use
 * independently mapped protection hardware, not these sense outputs.
 * Match Rel17 compctl(0) which disables the sensing comparators.
 */
#ifdef AM13E_E62_IO_PLAN_V1
static void sense_comparators_off(void)
{
    DL_CMPSSLITE_disableModule(CMPSS0);
    DL_CMPSSLITE_disableModule(CMPSS1);
    DL_CMPSSLITE_disableModule(CMPSS3);
}
#endif

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
    if(initialized) {
        capture_stop();
#ifdef AM13E_E62_IO_PLAN_V1
        sense_comparators_off();
#endif
    }
    selected_code=0U;
    interval_us=0U;
    __set_PRIMASK(primask);
}

#ifdef AM13E_E62_IO_PLAN_V1
static unsigned phase_cmp_instance(unsigned logical)
{
    switch(logical) {
        case 1U:return AM13E_BEMF_PHASE1_CMPSS_IDX;
        case 2U:return AM13E_BEMF_PHASE2_CMPSS_IDX;
        case 3U:return AM13E_BEMF_PHASE3_CMPSS_IDX;
        default:bemf_fault();
    }
    __builtin_unreachable();
}
static CMPSS_LITE_Regs *phase_cmp(unsigned logical)
{
    switch(phase_cmp_instance(logical)) {
        case 0U:return CMPSS0;
        case 1U:return CMPSS1;
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
    /* Rel17 does not leave every phase comparator active while idle.
     * compctl() enables exactly the selected comparator at commutation.
     */
    DL_CMPSSLITE_disableModule(cmp);
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
    config.captureModeConfig.continouousOrOneShot=DL_ECAP_CONTINUOUS_CAPTURE_MODE;
    config.captureModeConfig.wrapOrStopAtEvent=DL_ECAP_EVENT_1;
    config.captureModeConfig.captureEvent1Polarity=DL_ECAP_EVENT_RISING_EDGE;
    config.captureModeConfig.resetCounter=true; /* reset only at init */
    config.captureModeConfig.enableCounterResetOnCaptureEvent1=false;
    config.captureModeConfig.reArm=true;
    config.interruptsConfig.interruptSourceEnableMask=0U;
    DL_ECAP_init(BEMF_ECAP,&config);
    /* Continuous rejected-edge capture MUST keep the commutation-
     * referenced TSCTR; do not let CEVT1 reset the counter.
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
#ifdef AM13E_E62_IO_PLAN_V1
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
#ifdef AM13E_E62_IO_PLAN_V1
    sense_comparators_off();
#endif
    if((x&3)!=0) {
#ifndef AM13E_E62_IO_PLAN_V1
        /* No invented analog net or comparator validity. */
        bemf_fault();
#else
        const unsigned phase=(unsigned)x&3U;
        const unsigned hw=phase_cmp_instance(phase);
        const DL_ECAP_INPUT input=hw==0U?
            DL_ECAP_INPUT_CMPSS0_CTRIPH:hw==1U?
            DL_ECAP_INPUT_CMPSS1_CTRIPH:DL_ECAP_INPUT_CMPSS3_CTRIPH;
        DL_ECAP_selectECAPInput(BEMF_ECAP,input);
        if ((BEMF_ECAP->ECCTL0 & ECAP_ECCTL0_INPUTSEL_MASK) !=
                (uint32_t)input)
            bemf_fault();
        DL_ECAP_setEventPolarity(BEMF_ECAP,DL_ECAP_EVENT_1,
             (x&4)?DL_ECAP_EVENT_FALLING_EDGE:DL_ECAP_EVENT_RISING_EDGE);
        DL_CMPSSLITE_enableModule(phase_cmp(phase));
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
    if(!initialized) bemf_fault();
    capture_stop();
    /* Rel17 laststep() with lock may call nextstep() at rest, with
     * ertm==0. Do not claim BEMF availability or manufacture a timeout
     * capture for a stationary motor. Real running starts with ertm>0.
     */
    if (ertm_us<=0) {
        interval_us=0U;
        __set_PRIMASK(primask);
        return;
    }
    interval_us=(uint32_t)ertm_us;
    rejected_events=0U;
    if(selected_code) {
        if (!calibration_done || capture_ticks_per_ms == 0U ||
            capture_timeout_ticks == 0U) bemf_fault();
#ifdef AM13E_E62_IO_PLAN_V1
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
    /* Do not touch an uninitialized/unpowered ECAP1 peripheral. */
    if(!initialized)bemf_fault();
    const uint16_t flags=DL_ECAP_getInterruptSource(BEMF_ECAP);
    DL_ECAP_clearInterrupt(BEMF_ECAP,flags&BEMF_FLAGS);
    DL_ECAP_clearGlobalInterrupt(BEMF_ECAP);
    if(!armed)return;
    /* Counter overflow is a timeout. CAP1 data is not valid here. */
    if(flags&DL_ECAP_ISR_SOURCE_CTROVF) {
        capture_stop();
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
        return;
    }
    if((flags&DL_ECAP_ISR_SOURCE_CEVT1)==0U ||
       (flags&~BEMF_FLAGS)!=0U)bemf_fault();
    if (!calibration_done || !capture_ticks_per_ms) bemf_fault();
    const uint32_t ticks=DL_ECAP_getEventTimeStamp(BEMF_ECAP,DL_ECAP_EVENT_1);
    const uint64_t us=((uint64_t)ticks*UINT64_C(1000)+
                      capture_ticks_per_ms/2U)/capture_ticks_per_ms;
    if(us==0U || us>INT32_MAX)bemf_fault();
    if (ticks >= capture_timeout_ticks) {
        capture_stop();
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
        return;
    }
    ++captured_events;
    /* Rel17 rejects crossings earlier than ival/2. Keep ECAP1
     * continuously capturing until the shared policy accepts an edge.
     * Capture counter is NOT reset on a rejected edge.
     */
    if(am13e_app_motor_on_bemf_event((int)us,0))
        capture_stop();
    else
        ++rejected_events;
}

/* 16kHz SysTick supervision. ECAP1 hardware provides the elapsed
 * time in actual ticks, so timeout does not depend on scheduler jitter.
 * A no-edge BEMF interval cannot wait for the 32-bit counter overflow.
 * Calling the unchanged Rel17 timeout policy preserves sync reset.
 */
void am13e_app_motor_bemf_tick(void)
{
    if (!initialized) return;
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
            calibration_start=DL_ECAP_getTimeStampCounter(BEMF_ECAP);
            calibration_ticks=1U;
            return;
        }
        if (++calibration_ticks == BEMF_CALIB_SYSTICKS+1U) {
            const uint32_t elapsed=DL_ECAP_getTimeStampCounter(BEMF_ECAP)-
                                   calibration_start;
            /* Complete 1ms reference. 8.192ms/32.768ms conversions
             * below retain noninteger ECAP MHz rates without drift.
             */
            if (elapsed < 10000U || elapsed > 200000U) bemf_fault();
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
    if (DL_ECAP_getTimeStampCounter(BEMF_ECAP) <
        capture_timeout_ticks) return;
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (armed && DL_ECAP_getTimeStampCounter(BEMF_ECAP) >=
                     capture_timeout_ticks) {
        capture_stop();
        am13e_app_motor_timing_cancel();
        (void)am13e_app_motor_on_bemf_event(0,1);
    }
    __set_PRIMASK(mask);
}
