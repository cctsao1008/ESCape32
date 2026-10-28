# Same MOTOR_OUTPUT_CONFIGURED=1 fixture as control.
# ONLY startup interval/filter timing differs; do not flash hardware.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sim_only_motor_output.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
    AM13E_SIM_ONLY_EXPERIMENTAL_EARLY_BEMF=1)
