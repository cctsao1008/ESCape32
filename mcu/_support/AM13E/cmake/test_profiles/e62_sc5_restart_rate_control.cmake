# Matched H12 control: same actual ADC observer, original H8 seed policy.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_restart_phase_rate.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_RESTART_RATE_OBSERVE_ONLY=1)
