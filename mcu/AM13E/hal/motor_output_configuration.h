/* Electrical board definitions, not an application enable/approval policy.
 * No production electrical value is inferred from the Reference defaults.
 * The pad provider maps either independent gate inputs, or explicitly
 * selected Rel17 PWM/enable inputs. No topology is silently substituted.
 */
#pragma once
#if (defined(AM13E_MOTOR_PWM_ENABLE_TOPOLOGY) || defined(AM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS)) && !defined(PWM_ENABLE)
#error "PWM/enable board routing requires the original PWM_ENABLE selector"
#endif
#if defined(AM13E_MOTOR_GATE_ACTIVE_LEVEL) || defined(AM13E_MOTOR_PAD_INVERT_MASK) || \
    defined(AM13E_MOTOR_OUTPUT_RED_TICKS) || defined(AM13E_MOTOR_OUTPUT_FED_TICKS) || \
    defined(AM13E_MOTOR_NFAULT_ACTIVE_LOW) || defined(AM13E_MOTOR_GATE_RESET_INACTIVE) || \
    defined(AM13E_MOTOR_MIN_DEAD_TICKS) || defined(AM13E_MOTOR_PWM_ENABLE_TOPOLOGY) || \
    defined(AM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS)
#define AM13E_MOTOR_OUTPUT_CONFIGURED 1
#if !defined(AM13E_MOTOR_GATE_ACTIVE_LEVEL) || !defined(AM13E_MOTOR_PAD_INVERT_MASK) || \
    !defined(AM13E_MOTOR_OUTPUT_RED_TICKS) || !defined(AM13E_MOTOR_OUTPUT_FED_TICKS) || \
    !defined(AM13E_MOTOR_MIN_DEAD_TICKS) || !defined(AM13E_MOTOR_NFAULT_ACTIVE_LOW) || \
    !defined(AM13E_MOTOR_GATE_RESET_INACTIVE)
#error "Motor outputs require explicit gate/pad polarity, RED/FED/minimum dead time, nFAULT and reset-off wiring"
#endif
#ifdef INVERTED_HIGH
_Static_assert((AM13E_MOTOR_PAD_INVERT_MASK & 0x15)==0x15,
    "INVERTED_HIGH requires all three A/PWM pads inverted in the explicit board definition");
#endif
#ifdef PWM_ENABLE
#if !defined(AM13E_MOTOR_PWM_ENABLE_TOPOLOGY) || AM13E_MOTOR_PWM_ENABLE_TOPOLOGY != 1
#error "PWM_ENABLE requires explicit topology 1: A=PWM, B=active-low phase enable, before pad inversion"
#endif
#ifndef AM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS
#error "PWM/enable requires the driver's guaranteed internal non-overlap minimum in ns"
#endif
_Static_assert(AM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS>=AM13E_MOTOR_MIN_DEAD_TICKS*10,
    "Driver internal switching non-overlap must meet the board minimum at 100 MHz TBCLK");
/* Board truth table, before explicit pad inversion: B=1 floats the phase;
 * B=0,A=1 sources; B=0,A=0 sinks. Driver owns its internal complementary
 * gate control. A/B are not exposed high/low transistor gates in this mode.
 */
#endif
_Static_assert((AM13E_MOTOR_GATE_ACTIVE_LEVEL==0 || AM13E_MOTOR_GATE_ACTIVE_LEVEL==1) &&
    AM13E_MOTOR_PAD_INVERT_MASK>=0 && AM13E_MOTOR_PAD_INVERT_MASK<=63 &&
    (AM13E_MOTOR_NFAULT_ACTIVE_LOW==0 || AM13E_MOTOR_NFAULT_ACTIVE_LOW==1) &&
    AM13E_MOTOR_GATE_RESET_INACTIVE==1,
    "Invalid motor gate polarity or missing external reset-inactive gate wiring");
#ifdef PWM_ENABLE
_Static_assert(AM13E_MOTOR_OUTPUT_FED_TICKS==AM13E_MOTOR_OUTPUT_RED_TICKS,
    "Rel17 PWM/enable requires symmetric A rising / B falling delay");
#else
_Static_assert(AM13E_MOTOR_OUTPUT_FED_TICKS==0,
    "Independent gate AQ uses FED=0 and compare-delayed B");
#endif
_Static_assert(
    AM13E_MOTOR_OUTPUT_RED_TICKS<16384 && AM13E_MOTOR_OUTPUT_FED_TICKS<16384 &&
    AM13E_MOTOR_MIN_DEAD_TICKS>0 &&
    AM13E_MOTOR_OUTPUT_RED_TICKS>=AM13E_MOTOR_MIN_DEAD_TICKS,
    "RED must meet the explicit board minimum dead time");
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
#ifdef PWM_ENABLE
#define AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS AM13E_MOTOR_OUTPUT_RED_TICKS
#else
#define AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS (AM13E_MOTOR_OUTPUT_RED_TICKS-AM13E_MOTOR_OUTPUT_FED_TICKS)
#endif
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
