# Isolated SIM_ONLY first-edge acceptance hypothesis; no production defaults.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_dynamic_affine.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1)
