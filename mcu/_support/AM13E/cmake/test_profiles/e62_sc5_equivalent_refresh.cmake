# Isolated experimental acquisition candidate. Never production/original.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_virtual_reference.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_EQUIVALENT_REFRESH=1)
