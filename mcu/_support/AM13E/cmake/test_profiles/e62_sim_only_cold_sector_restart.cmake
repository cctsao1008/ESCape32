# E62 experiment ONLY; no board rotor-angle sensing is asserted.
# Compare source-built exact stable-filter oldstep restore vs a cold sector
# restart, under the SAME native analog motor and flight-controller DShot.
# This may IMPROVE or WORSEN reverse torque. Always preserve FAIL evidence.
# Neither this profile nor resulting ELF may be flashed to physical hardware.
include("${CMAKE_CURRENT_LIST_DIR}/e62_sim_only_early_bemf_stable_filter.cmake")
list(APPEND AM13E_BOARD_BOARD_DEFINITIONS
    AM13E_SIM_ONLY_EXPERIMENTAL_COLD_RESTART=1)
