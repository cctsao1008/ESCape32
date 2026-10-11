# PARK + ERPM compile/link fixture only; NEVER FLASH.
include("${CMAKE_CURRENT_LIST_DIR}/park_synthetic_profile.cmake")
set(park_definitions ${AM13E_BOARD_BOARD_DEFINITIONS})
include("${CMAKE_CURRENT_LIST_DIR}/erpm_synthetic_profile.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS ${park_definitions})
