/* Option 1 raw ADC0/1/2 housekeeping acquisition. The proposed SOC map
 * is not frozen or PWM synchronized. Only an explicit board calibration
 * enables Rel17 voltage/temperature policy; current codes remain raw.
 * No phase-current gain, NTC curve or Vdc/2 tracking is inferred here. */
#include "analog_runtime.h"
#include "clock_backend.h"
#include "analog_calibration_plan.h"
#include "analog_sampling_backend.h"
#include "analog_reference.h"
#include "board_configuration.h"
#include "motor_backend.h"
#ifdef ANALOG_CHAN
#include "board_reference_io.h"
/* Analog input is an independently selected ADC0 channel, NOT the
 * Reference NTC/voltage channels, and not a fabricated GPIO route.
 * Board profile must provide the actual pin/mux/channel/VREF.
 */
#if !defined(AM13E_BOARD_SENSORS_CONFIGURED) || \
    !defined(AM13E_BOARD_ANALOG_INPUT_PINCM) || \
    !defined(AM13E_BOARD_ANALOG_INPUT_CHANNEL) || \
    !defined(AM13E_BOARD_ANALOG_INPUT_FULLSCALE) || \
    !defined(AM13E_BOARD_ANALOG_INPUT_VREF_MV)
#error "Analog input_mode=1 requires board-supplied ADC pin/channel/fullscale/VREF and calibrated sensor build"
#endif
_Static_assert(AM13E_BOARD_ANALOG_INPUT_PINCM>0U &&
               AM13E_BOARD_ANALOG_INPUT_PINCM<107U &&
               AM13E_PIN_AVAILABLE(AM13E_BOARD_ANALOG_INPUT_PINCM) &&
               !AM13E_PIN_SERIAL(AM13E_BOARD_ANALOG_INPUT_PINCM) &&
               !AM13E_PIN_TELEM(AM13E_BOARD_ANALOG_INPUT_PINCM) &&
               !AM13E_PIN_HALL(AM13E_BOARD_ANALOG_INPUT_PINCM) &&
               !AM13E_PIN_AUX(AM13E_BOARD_ANALOG_INPUT_PINCM),
               "Analog receiver conflicts with selected Reference IO");
_Static_assert(AM13E_BOARD_ANALOG_INPUT_CHANNEL<=31U &&
               AM13E_BOARD_ANALOG_INPUT_FULLSCALE>=1U &&
               AM13E_BOARD_ANALOG_INPUT_FULLSCALE<=UINT16_MAX &&
               AM13E_BOARD_ANALOG_INPUT_VREF_MV>=1U &&
               AM13E_BOARD_ANALOG_INPUT_VREF_MV<=5000U,
               "Analog receiver ADC channel, resolution or VREF invalid");
#endif
/* Rel17 owns the public adctrig() declaration; include its canonical API. */
#include "common.h"
#include <soc.h>
#include <dl_adc.h>
#include <dl_gpio.h>
#include <dl_common.h>
#include <dl_sysctl.h>
#include <stddef.h>

/* Volatile because the hardware ISR produces both samples and PendSV
 * (original Rel17) initiates conversions. No fake or default samples.
 */
static volatile uint32_t adc_initialized;
static volatile uint32_t adc_inflight;
static volatile uint32_t adc_sample_seq;
static volatile AM13E_AdcRaw adc_latest;
static volatile uint32_t adc_have_sample;
static uint32_t acquisition_tick;
static volatile AM13E_BemfReference reference_latest;
static volatile uint32_t reference_valid;
#ifdef AM13E_BOARD_SC5_CALIBRATION
#if !defined(AM13E_BOARD_ADC_REFERENCE_MODE) || !defined(AM13E_BOARD_ADC_PACKAGE_QFN48)
#error "SC5 requires explicit native ADC reference mode and package contract"
#endif
_Static_assert(AM13E_BOARD_ADC_PACKAGE_QFN48==0 || AM13E_BOARD_ADC_PACKAGE_QFN48==1,
               "ADC package contract must be binary");
_Static_assert(AM13E_BOARD_ADC_REFERENCE_MODE==DL_SYSCTL_VREF_EXTERNAL_FULL_SCALE ||
               AM13E_BOARD_ADC_REFERENCE_MODE==DL_SYSCTL_VREF_EXTERNAL_HALF_SCALE ||
               AM13E_BOARD_ADC_REFERENCE_MODE==DL_SYSCTL_VREF_INTERNAL_3_3_V ||
               AM13E_BOARD_ADC_REFERENCE_MODE==DL_SYSCTL_VREF_INTERNAL_2_5_V,
               "Unsupported native ADC reference mode");
_Static_assert(!AM13E_BOARD_ADC_PACKAGE_QFN48 ||
               AM13E_BOARD_ADC_REFERENCE_MODE==DL_SYSCTL_VREF_EXTERNAL_FULL_SCALE,
               "QFN48 uses VDDA/VSSA and cannot use internal or half-scale reference");
static const AM13E_BemfCalibration reference_cal=AM13E_BOARD_SC5_CALIBRATION;
#endif

static void adc_fail_closed(void)
{
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
}

/* Reference ADC0 ISR and Rel17 housekeeping adapter; MCU sequencer
 * register programming belongs to the reusable analog_sampling_backend.
 */
static volatile uint32_t completed;
static uint16_t pending[3][3];
#ifdef ANALOG_CHAN
static uint16_t pending_analog;
#endif
void am13e_app_adc_init(void)
{
    if(adc_initialized || __get_PRIMASK()!=1U)adc_fail_closed();
#ifdef AM13E_BOARD_ADC_REFERENCE_MODE
    DL_SYSCTL_setVREF(AM13E_BOARD_ADC_REFERENCE_MODE);
    if(SYSCTL->SOCLOCK.ANAREFCTL!=(uint32_t)AM13E_BOARD_ADC_REFERENCE_MODE)adc_fail_closed();
#endif
    const unsigned pins[]={AM13E_CURRENT_U_PINCM,AM13E_CURRENT_V_PINCM,
        AM13E_CURRENT_W_PINCM,AM13E_BEMF_U_PINCM,AM13E_BEMF_V_PINCM,
        AM13E_BEMF_W_PINCM,AM13E_VBUS_PINCM,AM13E_IBUS_PINCM,AM13E_NTC_PINCM};
    for(unsigned i=0;i<sizeof pins/sizeof pins[0];++i)
        DL_GPIO_initPeripheralAnalogFunction(pins[i]);
#ifdef ANALOG_CHAN
    DL_GPIO_initPeripheralAnalogFunction(AM13E_BOARD_ANALOG_INPUT_PINCM);
    const int extra=AM13E_BOARD_ANALOG_INPUT_CHANNEL;
#else
    const int extra=-1;
#endif
    if(!am13e_mcu_adc_scan_initialize(ADC0,AM13E_ADC0_CURRENT_CHANNEL,
         AM13E_ADC0_PHASE_CHANNEL,AM13E_ADC0_IBUS_CHANNEL,extra,AM13E_APP_MCLK_HZ) ||
       !am13e_mcu_adc_scan_initialize(ADC1,AM13E_ADC1_CURRENT_CHANNEL,
         AM13E_ADC1_VBUS_CHANNEL,AM13E_ADC1_PHASE_CHANNEL,-1,AM13E_APP_MCLK_HZ) ||
       !am13e_mcu_adc_scan_initialize(ADC2,AM13E_ADC2_CURRENT_CHANNEL,
         AM13E_ADC2_PHASE_CHANNEL,AM13E_ADC2_NTC_CHANNEL,-1,AM13E_APP_MCLK_HZ))
        adc_fail_closed();
    completed=0U;adc_initialized=1U;
    NVIC_ClearPendingIRQ(ADC0_INT1_INT_IRQn);NVIC_ClearPendingIRQ(ADC1_INT1_INT_IRQn);
    NVIC_ClearPendingIRQ(ADC2_INT1_INT_IRQn);
    NVIC_SetPriority(ADC0_INT1_INT_IRQn,1U);NVIC_EnableIRQ(ADC0_INT1_INT_IRQn);
    NVIC_SetPriority(ADC1_INT1_INT_IRQn,1U);NVIC_EnableIRQ(ADC1_INT1_INT_IRQn);
    NVIC_SetPriority(ADC2_INT1_INT_IRQn,1U);NVIC_EnableIRQ(ADC2_INT1_INT_IRQn);
}
void adctrig(void)
{
    if(!adc_initialized)adc_fail_closed();
#ifdef AM13E_BOARD_SC5_CALIBRATION
    /* Existing PendSV housekeeping is the sole producer. Arithmetic stays
     * outside priority-0 commutation/capture IRQs and global masking. */
    AM13E_AdcRaw raw;
    AM13E_BemfReference ref;
    if(am13e_app_adc_get_raw(&raw) &&
       (!reference_valid || reference_latest.sample!=raw.sample_count ||
        (uint32_t)(tick-reference_latest.acquisition_tick)>reference_latest.max_age_ticks)) {
        ref.sample=raw.sample_count;ref.acquisition_tick=raw.acquisition_tick;
        ref.completion_tick=raw.completion_tick;ref.max_age_ticks=reference_cal.max_age_ticks;
        const int valid=am13e_bemf_reference_fresh(&ref,tick) &&
              am13e_bemf_reference_convert(&reference_cal,raw.vbus,ref.code);
        const uint32_t mask=__get_PRIMASK();__disable_irq();
        reference_valid=0U;
        if(valid){reference_latest=ref;reference_valid=1U;}
        __set_PRIMASK(mask);
    }
#endif
    if(adc_inflight || DL_ADC_isBusy(ADC0) || DL_ADC_isBusy(ADC1) || DL_ADC_isBusy(ADC2)){
        ++adc_latest.trigger_overruns;return;
    }
    completed=0U;adc_inflight=1U;
    acquisition_tick=tick;
    /* Sequential software requests, not simultaneous/PWM sampling. */
    DL_ADC_forceSequencer(ADC0,AM13E_ADC_SEQUENCE);
    DL_ADC_forceSequencer(ADC1,AM13E_ADC_SEQUENCE);
    DL_ADC_forceSequencer(ADC2,AM13E_ADC_SEQUENCE);
}

/* TI startup_gcc_arm.c actual ADC0/INT1 vector. */

/* Optional explicit board transfer model; never an automatic calibration. */
#ifdef AM13E_BOARD_SENSORS_CONFIGURED
#if AM13E_BOARD_SENSORS_CONFIGURED != 1
#error "AM13E_BOARD_SENSORS_CONFIGURED must be 1"
#endif
#if !defined(AM13E_BOARD_ADC_FULLSCALE) || \
    !defined(AM13E_BOARD_ADC_VREF_MV) || \
    !defined(AM13E_BOARD_NTC_SUPPLY_MV) || \
    !defined(AM13E_BOARD_VBUS_TOP_OHMS) || \
    !defined(AM13E_BOARD_VBUS_BOTTOM_OHMS) || \
    !defined(AM13E_BOARD_NTC_MODEL)
#error "AM13E reference calibrated ADC requires Vref, fullscale, VBUS divider and NTC model"
#endif
_Static_assert(AM13E_BOARD_NTC_MODEL>=1 && AM13E_BOARD_NTC_MODEL<=4,
               "Unsupported Rel17 NTC10K3455 curve selection");
static const AM13E_AdcCalibration adc_board_cal={
    AM13E_BOARD_ADC_FULLSCALE,
    AM13E_BOARD_ADC_VREF_MV,
    AM13E_BOARD_NTC_SUPPLY_MV,
    AM13E_BOARD_VBUS_TOP_OHMS,
    AM13E_BOARD_VBUS_BOTTOM_OHMS
};
static int32_t board_ntc_qc(uint16_t ntc_mv)
{
    switch(AM13E_BOARD_NTC_MODEL) {
        case 1:return NTC10K3455UP2K((int)ntc_mv);
        case 2:return NTC10K3455LO2K((int)ntc_mv);
        case 3:return NTC10K3455UP10K((int)ntc_mv);
        case 4:return NTC10K3455LO10K((int)ntc_mv);
        default:adc_fail_closed();return 0;
    }
}
#endif

static void adc_complete(unsigned instance,ADC_Regs *adc,ADC_RESULT_Regs *result)
{
    const uint32_t bit=1U<<instance;
    if(!adc_initialized || !adc_inflight || (completed&bit) ||
       !DL_ADC_getInterruptStatus(adc,AM13E_ADC_IRQ) ||
       !DL_ADC_getInterruptResultReadyStatus(adc,AM13E_ADC_IRQ))adc_fail_closed();
    for(unsigned soc=0U;soc<3U;++soc)
        pending[instance][soc]=DL_ADC_readResult(result,(DL_ADC_SOC_NUMBER)soc);
#ifdef ANALOG_CHAN
    if(instance==0U)pending_analog=DL_ADC_readResult(result,DL_ADC_SOC_NUMBER3);
#endif
    DL_ADC_clearInterruptStatus(adc,AM13E_ADC_IRQ);
    completed|=bit;
    if(completed!=7U)return;
    ++adc_sample_seq;
    adc_latest.current_v=pending[0][0];adc_latest.phase_a=pending[0][1];
    adc_latest.ibus=pending[0][2];adc_latest.current_w=pending[1][0];
    adc_latest.vbus=pending[1][1];adc_latest.phase_b=pending[1][2];
    adc_latest.current_u=pending[2][0];adc_latest.phase_c=pending[2][1];
    adc_latest.ntc=pending[2][2];
    ++adc_latest.sample_count;
    adc_latest.acquisition_tick=acquisition_tick;
    adc_latest.completion_tick=tick;
    adc_have_sample=1U;
    ++adc_sample_seq;
#ifdef AM13E_BOARD_SENSORS_CONFIGURED
    /* Feed the ACTUAL Rel17 smoothing/temperature/voltage protection
     * only after all three scans complete the raw epoch.
     * Board NTC and VBUS calibration values are required. Phase/IBUS
     * codes remain raw until a current transfer model is provided;
     * they are not substituted for Rel17 calibrated current units.
     */
    AM13E_AdcScaled scaled;
    if(!am13e_adc_scale_pair(&adc_board_cal,
           adc_latest.ntc,adc_latest.vbus,
           &scaled)) adc_fail_closed();
    int analog_mv=0;
#ifdef ANALOG_CHAN
    uint16_t sample_mv=0U;
    if(!am13e_adc_receiver_millivolts(
            pending_analog,AM13E_BOARD_ANALOG_INPUT_FULLSCALE,
            AM13E_BOARD_ANALOG_INPUT_VREF_MV,&sample_mv))
        adc_fail_closed();
    analog_mv=(int)sample_mv;
#endif
    /* Do NOT reproduce Rel17 analog smoothing/throttle policy here.
     * src/main.c::adcdata() retains its original exact scaling path.
     */
    adcdata(board_ntc_qc(scaled.ntc_normalized_mv),0,
            (int)scaled.vbus_centivolts,0,analog_mv);
#endif
    adc_inflight = 0U;
}

void ADC0_INT1_IRQHandler(void){adc_complete(0U,ADC0,ADC0RESULT);}
void ADC1_INT1_IRQHandler(void){adc_complete(1U,ADC1,ADC1RESULT);}
void ADC2_INT1_IRQHandler(void){adc_complete(2U,ADC2,ADC2RESULT);}
int am13e_app_adc_get_raw(AM13E_AdcRaw *out)
{
    if(out==NULL || !adc_have_sample)return 0;
    for(unsigned attempt=0;attempt<2U;++attempt){
        const uint32_t a=adc_sample_seq;if(a&1U)return 0;
        *out=adc_latest;
        if(a==adc_sample_seq)return 1;
    }
    return 0; /* Never spin behind a preempted lower-priority writer. */
}
int am13e_app_adc_get_reference(AM13E_BemfReference *out)
{
    if(!out)return 0;
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    const int valid=reference_valid!=0U;
    if(valid)*out=reference_latest;
    __set_PRIMASK(mask);
    return valid && am13e_bemf_reference_fresh(out,tick);
}
