/* E62 AM13E23019 ESCape32 Rel17: nonblocking ADC raw acquisition.
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
#include "adc_runtime.h"
#include "clock_backend.h"
#include <soc.h>
#include <dl_adc.h>
#include <dl_gpio.h>
#include <dl_common.h>
#include <stddef.h>

#define AM13E_ADC                     ADC0
#define AM13E_ADC_RESULTS             ADC0RESULT
#define AM13E_ADC_NTC_PINCM           IOMUX_PINCM_PA6
#define AM13E_ADC_VBUS_PINCM          IOMUX_PINCM_PA28
#define AM13E_ADC_NTC_SOC             DL_ADC_SOC_NUMBER0
#define AM13E_ADC_VBUS_SOC            DL_ADC_SOC_NUMBER1
#define AM13E_ADC_SEQUENCE           DL_ADC_SEQ_NUMBER1
#define AM13E_ADC_IRQ                DL_ADC_INT_NUMBER1
#define AM13E_ADC_ACQ_WINDOW_CYCLES   UINT32_C(640)

_Static_assert(IOMUX_PINCM_PA6 == 6 && IOMUX_PINCM_PA28 == 28,
               "AM13E ADC inputs changed unexpectedly");
_Static_assert(AM13E_ADC_ACQ_WINDOW_CYCLES >= DL_SAMPLEWINDOW_MIN &&
               AM13E_ADC_ACQ_WINDOW_CYCLES <= DL_SAMPLEWINDOW_MAX,
               "ADC acquisition window exceeds SDK limits");

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

void am13e_app_adc_init(void)
{
    DL_ADC_Config config;

    /* ADC0 is exclusively reserved for FW1 PA6/PA28 slow monitoring.
     * Preserve GPIO1/PB14, GPIO1/PB15 and power-stage pin ownership.
     */
    DL_GPIO_initPeripheralAnalogFunction(AM13E_ADC_NTC_PINCM);
    DL_GPIO_initPeripheralAnalogFunction(AM13E_ADC_VBUS_PINCM);

    DL_ADC_reset(AM13E_ADC);
    DL_ADC_enablePower(AM13E_ADC);
    if (!DL_ADC_isPowerEnabled(AM13E_ADC)) adc_fail_closed();

    DL_ADC_initParamsSetDefault(&config);
    /* Input clock must be qualified with board clocks. Nominal 200MHz
     * MCLK / 8 = 25MHz ADC clock; this is NOT a VREF/calibration choice.
     */
    config.coreConfig.clkPrescale = DL_ADC_CLOCK_DIVIDE_8_0;
    config.socConfig[AM13E_ADC_NTC_SOC].channel = DL_ADC_CH_ADCIN17;
    config.socConfig[AM13E_ADC_VBUS_SOC].channel = DL_ADC_CH_ADCIN11;
    config.seqConfig.endSocNumber = AM13E_ADC_VBUS_SOC;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].enableSequencer = true;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].sampleWindow =
        AM13E_ADC_ACQ_WINDOW_CYCLES;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].trigger =
        DL_ADC_TRIGGER_SOFTWARE;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].socStartNumber =
        AM13E_ADC_NTC_SOC;
    config.intConfig.pulseMode = DL_ADC_PULSE_END_OF_CONV;
    config.intConfig.intNConfig[AM13E_ADC_IRQ].enableInterrupt = true;
    config.intConfig.intNConfig[AM13E_ADC_IRQ].trigger = AM13E_ADC_VBUS_SOC;
    DL_ADC_init(AM13E_ADC, &config);

    /* TI DL_ADC_powerUp() requires >=500us analog stabilization. */
    DL_Common_delayCycles(AM13E_APP_MCLK_HZ / UINT32_C(2000));

    DL_ADC_clearInterruptStatus(AM13E_ADC, AM13E_ADC_IRQ);
    adc_initialized = 1U;
    NVIC_SetPriority(ADC0_INT1_INT_IRQn, 1U);
    NVIC_EnableIRQ(ADC0_INT1_INT_IRQn);
    /* Boot PRIMASK remains set until the real safe-enable barrier. */
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
