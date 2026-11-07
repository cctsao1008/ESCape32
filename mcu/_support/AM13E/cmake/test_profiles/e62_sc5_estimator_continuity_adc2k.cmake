# Native fresh-invalid isolation only. Not a production acquisition cadence.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_estimator_continuity.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_ADC_INTERLEAVE=1)
