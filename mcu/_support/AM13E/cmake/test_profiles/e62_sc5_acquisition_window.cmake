# H6 SIM_ONLY acceleration acquisition; no production default or P1 change.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_first_edge_seed.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_ACQUISITION_QUARTER=1)
