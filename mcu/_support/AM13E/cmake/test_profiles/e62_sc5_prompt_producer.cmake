# Isolated SIM_ONLY producer-latency correction; ADC cadence unchanged.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_dynamic_affine.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1)
