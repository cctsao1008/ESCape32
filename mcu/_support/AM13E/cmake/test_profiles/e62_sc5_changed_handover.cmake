# Isolated SIM_ONLY acquisition experiment; not a physical settling calibration.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_equivalent_refresh.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
  AM13E_EXPERIMENTAL_SC5_CHANGED_REFRESH=1
  AM13E_EXPERIMENTAL_SC5_SETTLE_ZEROS=2)
