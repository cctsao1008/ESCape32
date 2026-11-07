# Independent2kHz diagnostic cadence, not normal acquisition or calibration.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_estimator_continuity_prompt.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_ADC_INTERLEAVE=1)
