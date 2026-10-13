/* Synthetic register-test data ONLY. No E62 electrical specifications. */
#pragma once
#define AM13E_BOARD_ADC_REFERENCE_MODE DL_SYSCTL_VREF_EXTERNAL_FULL_SCALE
#define AM13E_BOARD_ADC_PACKAGE_QFN48 0
#define AM13E_BOARD_SC5_CALIBRATION { \
 .adc_span_uv=2500000U,.adc_gain_ppm=1000000U, \
 .bus_num=1U,.bus_den=16U,.phase_num={1U,1U,1U},.phase_den={16U,20U,25U}, \
 .vdda_uv=3300000U,.dac_gain_ppm=1000000U, \
 .bus_min_uv=1000U,.bus_max_uv=50000000U,.max_age_ticks=32U }
