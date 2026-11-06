# Independent experimental control: same acquisition fix without the journal.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sc5_equivalent_refresh.cmake")
list(REMOVE_ITEM AM13E_BOARD_BOARD_DEFINITIONS AM13E_REFERENCE_DIAGNOSTICS=1)
