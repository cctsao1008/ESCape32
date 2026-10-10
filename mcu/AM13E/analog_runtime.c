/* AM13E reference AM13E23019 ESCape32 Rel17: nonblocking ADC raw acquisition.
 *
 * HW Architecture Baseline v1.6: PA6 -> NTC_MAIN (A0_17),
 * PA28 -> VBUS_SENSE (A0_11). Physical ADC mux mapping verified against
 * TI AM13E23019 datasheet Table 5-2; driver calls are SDK 26.01.
 *
 * This file does NOT invent an NTC beta/pull-up, voltage-divider ratio,
 * ADC reference voltage, or current sensor. Those require board data.
 * It does NOT feed uncalibrated values into Rel17 adcdata() or assert
 * that temperature/voltage protection is functional.
 * No MCPWM/PB13 outputs are configured or enabled here.
 */
#include "analog_runtime.h"
#include "clock_backend.h"
#include "analog_calibration_plan.h"
#include "analog_sampling_backend.h"
#include "analog_reference.h"
#include "board_configuration.h"
/* Rel17 owns the public adctrig() declaration; include its canonical API. */
#include "common.h"
#include <soc.h>
#include <dl_adc.h>
#include <dl_gpio.h>
#include <dl_common.h>
#include <stddef.h>

/* Volatile because the hardware ISR produces both samples and PendSV
 * (original Rel17) initiates conversions. No fake or default samples.
 */
static volatile uint32_t adc_initialized;
static volatile uint32_t adc_inflight;
static volatile uint32_t adc_sample_seq;
static volatile AM13E_AdcRaw adc_latest;

static void adc_fail_closed(void)
{
    __disable_irq();
    for (;;) { __NOP(); }
}

/* Reference ADC0 ISR and Rel17 housekeeping adapter; MCU sequencer
 * register programming belongs to the reusable analog_sampling_backend.
 */
void am13e_app_adc_init(void)
{
    const AM13E_AdcPairRoute reference_route = {
        .adc = AM13E_ADC,
        .first_pincm = AM13E_ADC_NTC_PINCM,
        .second_pincm = AM13E_ADC_VBUS_PINCM,
        .first_channel = AM13E_ADC_NTC_CHANNEL,
        .second_channel = AM13E_ADC_VBUS_CHANNEL,
        .first_soc = AM13E_ADC_NTC_SOC,
        .second_soc = AM13E_ADC_VBUS_SOC,
        .sequencer = AM13E_ADC_SEQUENCE,
        .interrupt = AM13E_ADC_IRQ,
        .clock_prescale = DL_ADC_CLOCK_DIVIDE_8_0,
        .acquisition_cycles = AM13E_ADC_ACQ_WINDOW_CYCLES
    };

    /* ADC0 is reserved for this reference PA6/PA28 slow monitoring.
     * Never disturb GPIO1/PB14 input, PB15 fault, or gate output pads.
     */
    if (!am13e_mcu_adc_pair_initialize(&reference_route, AM13E_APP_MCLK_HZ))
        adc_fail_closed();

    adc_initialized = 1U;
    NVIC_SetPriority(ADC0_INT1_INT_IRQn, 1U);
    NVIC_EnableIRQ(ADC0_INT1_INT_IRQn);
    /* Boot PRIMASK remains set until the motor safe-enable barrier. */
}

/* Invoked at Rel17's original 1kHz PendSV housekeeping point. */
void adctrig(void)
{
    if (!adc_initialized) adc_fail_closed();
    if (adc_inflight || DL_ADC_isBusy(AM13E_ADC)) {
        ++adc_latest.trigger_overruns;
        return;
    }
    adc_inflight = 1U;
    DL_ADC_forceSequencer(AM13E_ADC, AM13E_ADC_SEQUENCE);
}

/* TI startup_gcc_arm.c actual ADC0/INT1 vector. */

/* The AM13E reference IO plan identifies PA6/PA28 channels but NOT the board's
 * Vref, voltage divider, NTC supply/pull-up or temperature curve.
 * No fabricated physical measurements may feed Rel17 adcdata().
 * A reviewed board profile must provide all six independent values.
 */
#ifdef AM13E_BOARD_SENSORS_CALIBRATED
#if AM13E_BOARD_SENSORS_CALIBRATED != 1
#error "AM13E_BOARD_SENSORS_CALIBRATED must be 1"
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

void ADC0_INT1_IRQHandler(void)
{
    if (!adc_initialized || !adc_inflight ||
        !DL_ADC_getInterruptStatus(AM13E_ADC, AM13E_ADC_IRQ) ||
        !DL_ADC_getInterruptResultReadyStatus(AM13E_ADC, AM13E_ADC_IRQ)) {
        adc_fail_closed();
    }

    /* The pair is delivered together. Sequence increments bracket writes,
     * providing coherent debugging reads without blocking the ISR.
     */
    ++adc_sample_seq;
    adc_latest.ntc_main_adc0_in17 =
        DL_ADC_readResult(AM13E_ADC_RESULTS, AM13E_ADC_NTC_SOC);
    adc_latest.vbus_adc0_in11 =
        DL_ADC_readResult(AM13E_ADC_RESULTS, AM13E_ADC_VBUS_SOC);
    ++adc_latest.sample_count;
    ++adc_sample_seq;
#ifdef AM13E_BOARD_SENSORS_CALIBRATED
    /* Feed the ACTUAL Rel17 smoothing/temperature/voltage protection
     * only after one coherent two-channel ADC sequence has completed.
     * The board's physical NTC and VBUS calibration values are required.
     * No current sensor appears in IO Plan v1.0; original SENS_CNT is
     * restricted to one (voltage) in this calibrated build.
     */
    AM13E_AdcScaled scaled;
    if(!am13e_adc_scale_pair(&adc_board_cal,
           adc_latest.ntc_main_adc0_in17,adc_latest.vbus_adc0_in11,
           &scaled)) adc_fail_closed();
    adcdata(board_ntc_qc(scaled.ntc_normalized_mv),0,
            (int)scaled.vbus_centivolts,0,0);
#endif
    DL_ADC_clearInterruptStatus(AM13E_ADC, AM13E_ADC_IRQ);
    adc_inflight = 0U;
}

int am13e_app_adc_get_raw(AM13E_AdcRaw *out)
{
    uint32_t a, b;
    if (out == NULL || adc_latest.sample_count == 0U) return 0;
    for (;;) {
        a = adc_sample_seq;
        if (a & 1U) continue;
        out->ntc_main_adc0_in17 = adc_latest.ntc_main_adc0_in17;
        out->vbus_adc0_in11 = adc_latest.vbus_adc0_in11;
        out->sample_count = adc_latest.sample_count;
        out->trigger_overruns = adc_latest.trigger_overruns;
        b = adc_sample_seq;
        if (a == b && !(b & 1U)) return 1;
    }
}
