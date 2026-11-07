# Integration fixture combining independently isolated policy and SC5 fix.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_acquisition_estimator.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_CONTINUOUS_EQUIVALENT=1)
