# H13: actual coast phase-sequence acquisition; ideal-code SIM_ONLY scope.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_estimator_continuity_prompt.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_COAST_PHASE_ACQUIRE=1)
