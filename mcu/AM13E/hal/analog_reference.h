/*
 * AM13E REFERENCE ADC route only (not an AM13E23019 silicon default).
 * PA6=NTC_MAIN / ADC0_IN17, PA28=VBUS / ADC0_IN11.
 * These two channels are raw-only until separate real electrical
 * calibration is supplied. Never infer analog scaling from this header.
 */
#pragma once
#ifndef AM13E
#error "AM13E reference ADC route requires AM13E"
#endif
#include <soc.h>
#include <dl_adc.h>
#include <stdint.h>
#define AM13E_ADC                    ADC0
#define AM13E_ADC_RESULTS            ADC0RESULT
#define AM13E_ADC_NTC_PINCM          IOMUX_PINCM_PA6
#define AM13E_ADC_VBUS_PINCM         IOMUX_PINCM_PA28
#define AM13E_ADC_NTC_SOC            DL_ADC_SOC_NUMBER0
#define AM13E_ADC_VBUS_SOC           DL_ADC_SOC_NUMBER1
#define AM13E_ADC_SEQUENCE          DL_ADC_SEQ_NUMBER1
#define AM13E_ADC_IRQ               DL_ADC_INT_NUMBER1
#define AM13E_ADC_ACQ_WINDOW_CYCLES  UINT32_C(640)
#define AM13E_ADC_NTC_CHANNEL        DL_ADC_CH_ADCIN17
#define AM13E_ADC_VBUS_CHANNEL       DL_ADC_CH_ADCIN11

_Static_assert(IOMUX_PINCM_PA6 == 6 && IOMUX_PINCM_PA28 == 28,
               "AM13E reference ADC physical pins changed");
_Static_assert(AM13E_ADC_ACQ_WINDOW_CYCLES >= DL_SAMPLEWINDOW_MIN &&
               AM13E_ADC_ACQ_WINDOW_CYCLES <= DL_SAMPLEWINDOW_MAX,
               "Reference ADC acquisition window exceeds TI SDK limits");
