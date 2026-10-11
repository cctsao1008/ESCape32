#include "analog_sampling_backend.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_common.h>
#include <stddef.h>

int am13e_mcu_adc_pair_route_valid(const AM13E_AdcPairRoute *route)
{
    if (route == NULL ||
        (route->adc != ADC0 && route->adc != ADC1 &&
         route->adc != ADC2) ||
        route->first_pincm >= 107U ||
        route->second_pincm >= 107U ||
        route->first_pincm == route->second_pincm ||
        route->first_channel == route->second_channel ||
        (uint32_t)route->first_channel > 31U ||
        (uint32_t)route->second_channel > 31U ||
        route->second_soc != route->first_soc + 1U ||
        route->second_soc > DL_ADC_SOC_NUMBER15 ||
        route->sequencer > DL_ADC_SEQ_NUMBER4 ||
        route->interrupt > DL_ADC_INT_NUMBER4 ||
        route->clock_prescale > DL_ADC_CLOCK_DIVIDE_8_5 ||
        route->acquisition_cycles < DL_SAMPLEWINDOW_MIN ||
        route->acquisition_cycles > DL_SAMPLEWINDOW_MAX ||
        route->analog_enabled > 1U ||
        (route->analog_enabled &&
         (route->analog_pincm >= 107U ||
          route->analog_pincm == route->first_pincm ||
          route->analog_pincm == route->second_pincm ||
          (uint32_t)route->analog_channel > 31U ||
          route->analog_channel == route->first_channel ||
          route->analog_channel == route->second_channel ||
          route->second_soc >= DL_ADC_SOC_NUMBER15 ||
          route->analog_soc != route->second_soc + 1U))) {
        return 0;
    }
    return 1;
}

int am13e_mcu_adc_pair_initialize(const AM13E_AdcPairRoute *route,
                                  uint32_t mclk_hz)
{
    if (!am13e_mcu_adc_pair_route_valid(route) ||
        __get_PRIMASK() != 1U ||
        mclk_hz < UINT32_C(2000)) {
        return 0;
    }

    DL_GPIO_initPeripheralAnalogFunction(route->first_pincm);
    DL_GPIO_initPeripheralAnalogFunction(route->second_pincm);
    if (route->analog_enabled)
        DL_GPIO_initPeripheralAnalogFunction(route->analog_pincm);

    DL_ADC_reset(route->adc);
    DL_ADC_enablePower(route->adc);
    if (!DL_ADC_isPowerEnabled(route->adc)) return 0;

    DL_ADC_Config config;
    DL_ADC_initParamsSetDefault(&config);
    config.coreConfig.clkPrescale = route->clock_prescale;
    config.socConfig[route->first_soc].channel = route->first_channel;
    config.socConfig[route->second_soc].channel = route->second_channel;
    if (route->analog_enabled)
        config.socConfig[route->analog_soc].channel = route->analog_channel;
    config.seqConfig.endSocNumber = route->analog_enabled ?
        route->analog_soc : route->second_soc;
    config.seqConfig.seqNConfig[route->sequencer].enableSequencer = true;
    config.seqConfig.seqNConfig[route->sequencer].sampleWindow =
        route->acquisition_cycles;
    config.seqConfig.seqNConfig[route->sequencer].trigger =
        DL_ADC_TRIGGER_SOFTWARE;
    config.seqConfig.seqNConfig[route->sequencer].socStartNumber =
        route->first_soc;
    config.intConfig.pulseMode = DL_ADC_PULSE_END_OF_CONV;
    config.intConfig.intNConfig[route->interrupt].enableInterrupt = true;
    config.intConfig.intNConfig[route->interrupt].trigger =
        route->analog_enabled ? route->analog_soc : route->second_soc;
    DL_ADC_init(route->adc, &config);

    /* Preserve original 500us analog stabilization against verified MCLK. */
    DL_Common_delayCycles(mclk_hz / UINT32_C(2000));
    DL_ADC_clearInterruptStatus(route->adc, route->interrupt);
    return 1;
}
