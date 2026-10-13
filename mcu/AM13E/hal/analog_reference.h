/* TI Option 1 proposed SOC map. Software housekeeping scans are not
 * PWM-synchronized FOC sampling or calibrated electrical measurements. */
#pragma once
#include "board_io.h"
#include <soc.h>
#include <dl_adc.h>
#define AM13E_ADC_SEQUENCE DL_ADC_SEQ_NUMBER1
#define AM13E_ADC_IRQ DL_ADC_INT_NUMBER1
#define AM13E_ADC_ACQ_WINDOW_CYCLES 640U /* Retained slow-monitoring window. */
#define AM13E_ADC0_CURRENT_CHANNEL DL_ADC_CH_ADCIN5
#define AM13E_ADC0_PHASE_CHANNEL DL_ADC_CH_ADCIN12
#define AM13E_ADC0_IBUS_CHANNEL DL_ADC_CH_ADCIN4
#define AM13E_ADC1_CURRENT_CHANNEL DL_ADC_CH_ADCIN4
#define AM13E_ADC1_VBUS_CHANNEL DL_ADC_CH_ADCIN12
#define AM13E_ADC1_PHASE_CHANNEL DL_ADC_CH_ADCIN2
#define AM13E_ADC2_CURRENT_CHANNEL DL_ADC_CH_ADCIN12
#define AM13E_ADC2_PHASE_CHANNEL DL_ADC_CH_ADCIN5
#define AM13E_ADC2_NTC_CHANNEL DL_ADC_CH_ADCIN19
_Static_assert(AM13E_ADC_ACQ_WINDOW_CYCLES>=DL_SAMPLEWINDOW_MIN &&
               AM13E_ADC_ACQ_WINDOW_CYCLES<=DL_SAMPLEWINDOW_MAX,
               "ADC monitoring window outside SDK limits");
