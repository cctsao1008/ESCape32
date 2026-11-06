# Diagnostic fixture only. Neither a product board nor an original ELF.
if(NOT E62_QEMU_ROOT OR NOT G13B2_SIM_ONLY_OPT_IN)
  message(FATAL_ERROR "SC5 virtual profile requires explicit QEMU fixture root and SIM opt-in")
endif()
if(G13B2_SIM_ONLY_IDEAL_BEMF_REFERENCE)
  message(FATAL_ERROR "Dynamic SC5 forbids the legacy initial-DAC fixture")
endif()
include("${E62_QEMU_ROOT}/tests/fixtures/g13b3l_sim_only_ramp.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
  AM13E_REFERENCE_DIAGNOSTICS=1
  AM13E_BOARD_ADC_REFERENCE_MODE=DL_SYSCTL_VREF_EXTERNAL_FULL_SCALE
  AM13E_BOARD_ADC_PACKAGE_QFN48=0
  AM13E_BOARD_ADC_FULLSCALE=4095
  AM13E_BOARD_ADC_VREF_MV=3300
  AM13E_BOARD_NTC_SUPPLY_MV=3300
  AM13E_BOARD_NTC_MODEL=3
  AM13E_BOARD_VBUS_TOP_OHMS=149000
  AM13E_BOARD_VBUS_BOTTOM_OHMS=11000
  "AM13E_BOARD_SC5_CALIBRATION={.adc_span_uv=3300000U,.adc_gain_ppm=1000000U,.bus_num=11U,.bus_den=160U,.phase_num={11U,11U,11U},.phase_den={80U,80U,80U},.vdda_uv=3300000U,.dac_gain_ppm=1000000U,.bus_min_uv=1000U,.bus_max_uv=50000000U,.max_age_ticks=32U}")
message(WARNING "SC5 virtual acquisition observer; NOT board calibration or production firmware")
