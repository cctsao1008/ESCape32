# H11 single-factor continuity control: H8 estimator + H10 prompt, H9 disabled.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_acquisition_estimator.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1)
