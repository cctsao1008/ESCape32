# Explicit virtual summing network; NOT a measured production calibration.
# VBUS pin=VBUS*11/160. Phase pin=1.41V+(VBUS/2+BEMF)*1/50.
# At24V this preserves the legacy1.65V center/.02BEMF gain.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_changed_handover.cmake")
list(FILTER AM13E_BOARD_BOARD_DEFINITIONS EXCLUDE REGEX "^AM13E_BOARD_SC5_CALIBRATION=")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
  "AM13E_BOARD_SC5_CALIBRATION={.adc_span_uv=3300000U,.adc_gain_ppm=1000000U,.bus_num=11U,.bus_den=160U,.phase_num={1U,1U,1U},.phase_den={50U,50U,50U},.phase_offset_uv={1410000,1410000,1410000},.vdda_uv=3300000U,.dac_gain_ppm=1000000U,.bus_min_uv=1000U,.bus_max_uv=50000000U,.max_age_ticks=32U}")
