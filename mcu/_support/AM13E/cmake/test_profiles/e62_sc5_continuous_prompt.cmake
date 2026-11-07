# Independent SC5 producer/continuity fixes with original acceptance policy.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_continuous_equivalent.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS AM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1)
