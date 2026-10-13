#pragma once
#include <stdint.h>
#include <dl_adc.h>
/* One native sequencer, SOC0..2 plus an optional explicit SOC3.
 * Fixed Option 1 resources are selected by analog_reference.h. */
int am13e_mcu_adc_scan_initialize(ADC_Regs *adc,DL_ADC_CHANNEL soc0,
    DL_ADC_CHANNEL soc1,DL_ADC_CHANNEL soc2,int soc3,uint32_t mclk_hz);
