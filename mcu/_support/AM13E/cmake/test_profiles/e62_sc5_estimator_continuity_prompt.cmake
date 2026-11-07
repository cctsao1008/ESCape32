# Integration only: isolated estimator + continuity + prompt producer fixes.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_estimator_continuity.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1)
