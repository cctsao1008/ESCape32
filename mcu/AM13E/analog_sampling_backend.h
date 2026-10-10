/*
 * Real TI AM13E ADC pair acquisition backend, without a board choice.
 * Board profile supplies both analog pinmux entries and physical channels.
 * No NTC, Vref, divider, current sensing, or calibration values are assumed.
 */
#pragma once
#ifndef AM13E
#error "AM13E ADC pair backend must not leak into legacy MCU targets"
#endif
#include <stdint.h>
#include <dl_adc.h>

typedef struct {
    ADC_Regs *adc;
    uint32_t first_pincm;
    uint32_t second_pincm;
    DL_ADC_CHANNEL first_channel;
    DL_ADC_CHANNEL second_channel;
    DL_ADC_SOC_NUMBER first_soc;
    DL_ADC_SOC_NUMBER second_soc;
    DL_ADC_SEQ_NUMBER sequencer;
    DL_ADC_INT_NUMBER interrupt;
    DL_ADC_CLK_PRESCALE clock_prescale;
    uint32_t acquisition_cycles;
    /* Optional board-selected third sample for original Rel17
     * input_mode=1; the selected Reference's NTC/VBUS route is unchanged.
     * All values are explicit, never inferred from the ADC number.
     */
    uint32_t analog_enabled;
    uint32_t analog_pincm;
    DL_ADC_CHANNEL analog_channel;
    DL_ADC_SOC_NUMBER analog_soc;
} AM13E_AdcPairRoute;

/* Validate a two-channel consecutive-SOC plan before writing registers.
 * The board still must prove that pinmux/channel combinations are physical.
 */
int am13e_mcu_adc_pair_route_valid(const AM13E_AdcPairRoute *route);

/* Caller retains PRIMASK and NVIC ownership. Returns 0 on any invalid
 * route/power failure. Never supplies synthetic ADC readings.
 * mclk_hz is nominal, separately validated by the selected clock provider.
 */
int am13e_mcu_adc_pair_initialize(const AM13E_AdcPairRoute *route,
                                  uint32_t mclk_hz);
