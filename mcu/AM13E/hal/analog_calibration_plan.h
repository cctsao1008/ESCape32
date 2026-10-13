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
/* Explicit electrical contract, in microvolts and rational gains. No defaults.
 * ADC/DAC gain calibration is ppm; ADC offset is in native code units.
 * VDDA is the CMPSS reference, independently of the effective ADC span. */
typedef struct {
    uint32_t adc_span_uv,adc_gain_ppm;
    int32_t adc_low_uv,adc_offset_code;
    uint32_t bus_num,bus_den;
    int32_t bus_offset_uv;
    uint32_t phase_num[3],phase_den[3];
    int32_t phase_offset_uv[3],comparator_correction_uv[3];
    uint32_t vdda_uv,dac_gain_ppm;
    int32_t dac_offset_uv;
    uint32_t bus_min_uv,bus_max_uv,max_age_ticks;
} AM13E_BemfCalibration;
typedef struct {
    uint16_t code[3];
    uint32_t sample,acquisition_tick,completion_tick,max_age_ticks;
} AM13E_BemfReference;
int am13e_bemf_reference_convert(const AM13E_BemfCalibration *cal,
                                uint16_t raw,uint16_t code[3]);
int am13e_bemf_reference_fresh(const AM13E_BemfReference *ref,uint32_t now);
