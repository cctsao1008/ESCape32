# Causally isolated SIM_ONLY estimator candidate. No physical qualification.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_acquisition_window.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_SIM_ONLY_ACQUISITION_DIRECT_INTERVAL=1)
