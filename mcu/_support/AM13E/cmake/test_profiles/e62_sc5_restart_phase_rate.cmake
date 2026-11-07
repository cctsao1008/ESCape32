# H12 first restart seed from real phase-ADC apparent rotation.
# Not a qualified direction sensor; 1kHz alias limitation is explicit.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_estimator_continuity_prompt.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_RESTART_PHASE_RATE=1)
list(APPEND libs m)
