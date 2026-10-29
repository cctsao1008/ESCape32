# Experimental CMPSS digital-filter sensitivity evaluation ONLY.
# Exact same Source + Board safety fixture as 3ms/quarter baseline;
# the only extra definition qualifies 2us switching transients.
# NEITHER this profile NOR the source-built firmware may be flashed.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sim_only_early_bemf.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
    AM13E_SIM_ONLY_BEMF_STABLE_FILTER=1)
