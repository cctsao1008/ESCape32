/* Electrical board definitions, not an application enable/approval policy.
 * No production electrical value is inferred from the Reference defaults.
 * Supported topology: six independent half-bridge gate inputs, mapped by
 * the existing pad provider. PWM_ENABLE driver topologies need their own
 * electrical adapter; they must not silently use this six-input mapping.
 */
#pragma once
#if defined(AM13E_MOTOR_GATE_ACTIVE_LEVEL) || defined(AM13E_MOTOR_PAD_INVERT_MASK) || \
    defined(AM13E_MOTOR_OUTPUT_RED_TICKS) || defined(AM13E_MOTOR_OUTPUT_FED_TICKS) || \
    defined(AM13E_MOTOR_NFAULT_ACTIVE_LOW) || defined(AM13E_MOTOR_GATE_RESET_INACTIVE) || \
    defined(AM13E_MOTOR_MIN_DEAD_TICKS)
#define AM13E_MOTOR_OUTPUT_CONFIGURED 1
#if !defined(AM13E_MOTOR_GATE_ACTIVE_LEVEL) || !defined(AM13E_MOTOR_PAD_INVERT_MASK) || \
    !defined(AM13E_MOTOR_OUTPUT_RED_TICKS) || !defined(AM13E_MOTOR_OUTPUT_FED_TICKS) || \
    !defined(AM13E_MOTOR_MIN_DEAD_TICKS) || !defined(AM13E_MOTOR_NFAULT_ACTIVE_LOW) || \
    !defined(AM13E_MOTOR_GATE_RESET_INACTIVE)
#error "Motor outputs require explicit gate/pad polarity, RED/FED/minimum dead time, nFAULT and reset-off wiring"
#endif
#ifdef PWM_ENABLE
#error "PWM_ENABLE requires a board-specific PWM/enable gate-driver topology adapter"
#endif
_Static_assert((AM13E_MOTOR_GATE_ACTIVE_LEVEL==0 || AM13E_MOTOR_GATE_ACTIVE_LEVEL==1) &&
    AM13E_MOTOR_PAD_INVERT_MASK>=0 && AM13E_MOTOR_PAD_INVERT_MASK<=63 &&
    (AM13E_MOTOR_NFAULT_ACTIVE_LOW==0 || AM13E_MOTOR_NFAULT_ACTIVE_LOW==1) &&
    AM13E_MOTOR_GATE_RESET_INACTIVE==1,
    "Invalid motor gate polarity or missing external reset-inactive gate wiring");
_Static_assert(AM13E_MOTOR_OUTPUT_FED_TICKS==0 &&
    AM13E_MOTOR_OUTPUT_RED_TICKS<16384 && AM13E_MOTOR_OUTPUT_FED_TICKS<16384 &&
    AM13E_MOTOR_MIN_DEAD_TICKS>0 &&
    AM13E_MOTOR_OUTPUT_RED_TICKS-AM13E_MOTOR_OUTPUT_FED_TICKS>=AM13E_MOTOR_MIN_DEAD_TICKS,
    "Independent gate AQ uses FED=0 and compare-delayed B; RED must meet the board minimum dead time");
/* Independent A/B preserves floating/sink/freewheel leg selection. Delaying
 * A rising and using a later B compare realizes the same gap on
 * both switching edges. Common-input complementary DB would drive float B.
 */
#undef AM13E_MOTOR_DB_RED_TICKS
#undef AM13E_MOTOR_DB_FED_TICKS
#undef AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS
#undef AM13E_MOTOR_DB_RED_INPUT
#undef AM13E_MOTOR_DB_FED_INPUT
#undef AM13E_MOTOR_DB_RED_POLARITY
#undef AM13E_MOTOR_DB_FED_POLARITY
#undef AM13E_MOTOR_DB_SWAP_A
#undef AM13E_MOTOR_DB_SWAP_B
#define AM13E_MOTOR_DB_RED_TICKS AM13E_MOTOR_OUTPUT_RED_TICKS
#define AM13E_MOTOR_DB_FED_TICKS AM13E_MOTOR_OUTPUT_FED_TICKS
#define AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS (AM13E_MOTOR_OUTPUT_RED_TICKS-AM13E_MOTOR_OUTPUT_FED_TICKS)
#define AM13E_MOTOR_DB_RED_INPUT 0
#define AM13E_MOTOR_DB_FED_INPUT 1
#define AM13E_MOTOR_DB_RED_POLARITY 0
#define AM13E_MOTOR_DB_FED_POLARITY 0
#define AM13E_MOTOR_DB_SWAP_A 0
#define AM13E_MOTOR_DB_SWAP_B 0
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
#define AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED 1
#endif
#undef AM13E_BOARD_PB13_ACTIVE_LEVEL
#define AM13E_BOARD_PB13_ACTIVE_LEVEL AM13E_MOTOR_GATE_ACTIVE_LEVEL
#else
#define AM13E_MOTOR_OUTPUT_CONFIGURED 0
#endif
