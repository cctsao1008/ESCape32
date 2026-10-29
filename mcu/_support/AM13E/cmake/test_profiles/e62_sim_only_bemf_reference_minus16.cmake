# DIAGNOSTIC ONLY: independent QEMU relative phase-sense offset causality.
# Not a board calibration / firmware release / actual measured DAC setting.
# This value is intentionally aligned to the model's artificial signed
# +/-16 ADC-code sense bias to isolate the comparator reference mechanism.
# Control remains the exact same source with the original SIM_ONLY 2048.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sim_only_early_bemf_stable_filter.cmake")
list(REMOVE_ITEM AM13E_BOARD_BOARD_DEFINITIONS
    AM13E_BEMF_INITIAL_DAC_CODE=2048)
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
    AM13E_BEMF_INITIAL_DAC_CODE=2032)
