# H5 SIM_ONLY interval-seed hypothesis, not a production startup guarantee.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_first_edge_quarter.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_FIRST_EDGE_SEED=1)
