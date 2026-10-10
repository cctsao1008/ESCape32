/* AM13E reference Rel17 ADC physical-unit normalization — pure C, host testable.
 * No board resistor, ADC reference, or thermistor curve is inferred here.
 * Voltage output = 0.01V/count (centivolts) for Rel17 adcdata().
 * Thermistor output = voltage in mV normalized to a 3.3V NTC supply,
 * for unchanged Rel17 NTC10K3455* conversion functions.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t adc_fullscale;
    uint32_t adc_vref_mv;
    uint32_t ntc_supply_mv;
    uint32_t divider_top_ohms;
    uint32_t divider_bottom_ohms;
} AM13E_AdcCalibration;
typedef struct {
    uint16_t ntc_normalized_mv;
    uint16_t vbus_centivolts;
} AM13E_AdcScaled;
/* Original Rel17 adcdata(...,a) expects receiver voltage in mV.
 * Requires board-qualified VREF/fullscale, not G431 defaults.
 * This converts only a physical ADC code; original adcdata() retains
 * analog smoothing/analog_min/max -> throttle policy.
 */
int am13e_adc_receiver_millivolts(uint16_t raw,uint32_t fullscale,
                                   uint32_t vref_mv,uint16_t *out);
int am13e_adc_scale_pair(const AM13E_AdcCalibration *calibration,
                         uint16_t ntc_raw,uint16_t vbus_raw,
                         AM13E_AdcScaled *out);
