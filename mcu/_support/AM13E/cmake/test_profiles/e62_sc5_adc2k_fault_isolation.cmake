# SIM_ONLY test cadence; no new calibration or safety-age values.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_dynamic_affine.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_ADC_INTERLEAVE=1)
