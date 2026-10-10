/* Editable AM13E reference-board software defaults.
 * -1: unknown polarity/topology; 0: unconfigured physical parameter.
 * No value in this file is proof of gate, OC, RED/FED or analog hardware.
 * Explicit -D values or a reviewed CMake Board Profile may override fields.
 * The existing electrical enable/VERIFIED opt-ins are deliberately ABSENT.
 */
#pragma once
#ifndef AM13E
#error "AM13E board configuration is not for legacy MCU builds"
#endif

/* Existing six motor pads, PB13/PB14/PB15 and ADC pin routing stay in
 * board_reference_io.h, board_motor_output_reference.c,
 * command_input_reference.h, motor_fault_route.c and analog_reference.h.
 */
#ifndef AM13E_BOARD_PB13_ACTIVE_LEVEL
#define AM13E_BOARD_PB13_ACTIVE_LEVEL (-1)
#endif
#ifndef AM13E_BOARD_GATE_PWM_INVERT_MASK
#define AM13E_BOARD_GATE_PWM_INVERT_MASK (-1)
#endif
#ifndef AM13E_BOARD_GATE_INPUTS_HIZ_SAFE
#define AM13E_BOARD_GATE_INPUTS_HIZ_SAFE 0
#endif
#ifndef AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN
#define AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN 0
#endif

/* Unverified dead-band: zero counts MUST NOT be used for live output. */
#ifndef AM13E_MOTOR_DB_RED_TICKS
#define AM13E_MOTOR_DB_RED_TICKS 0
#endif
#ifndef AM13E_MOTOR_DB_FED_TICKS
#define AM13E_MOTOR_DB_FED_TICKS 0
#endif
#ifndef AM13E_MOTOR_DB_RED_POLARITY
#define AM13E_MOTOR_DB_RED_POLARITY (-1)
#endif
#ifndef AM13E_MOTOR_DB_FED_POLARITY
#define AM13E_MOTOR_DB_FED_POLARITY (-1)
#endif
#ifndef AM13E_MOTOR_DB_RED_INPUT
#define AM13E_MOTOR_DB_RED_INPUT (-1)
#endif
#ifndef AM13E_MOTOR_DB_FED_INPUT
#define AM13E_MOTOR_DB_FED_INPUT (-1)
#endif
#ifndef AM13E_MOTOR_DB_SWAP_A
#define AM13E_MOTOR_DB_SWAP_A (-1)
#endif
#ifndef AM13E_MOTOR_DB_SWAP_B
#define AM13E_MOTOR_DB_SWAP_B (-1)
#endif
#ifndef AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS
#define AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS (-1)
#endif

/* Known 12-bit raw ADC code range does not imply calibrated VREF or NTC. */
#ifndef AM13E_BOARD_ADC_FULLSCALE
#define AM13E_BOARD_ADC_FULLSCALE 4095
#endif
#ifndef AM13E_BOARD_ADC_VREF_MV
#define AM13E_BOARD_ADC_VREF_MV 0
#endif
#ifndef AM13E_BOARD_NTC_SUPPLY_MV
#define AM13E_BOARD_NTC_SUPPLY_MV 0
#endif
#ifndef AM13E_BOARD_VBUS_TOP_OHMS
#define AM13E_BOARD_VBUS_TOP_OHMS 0
#endif
#ifndef AM13E_BOARD_VBUS_BOTTOM_OHMS
#define AM13E_BOARD_VBUS_BOTTOM_OHMS 0
#endif
#ifndef AM13E_BOARD_NTC_MODEL
#define AM13E_BOARD_NTC_MODEL 0
#endif

/* The existing PB15 nFAULT -> OST1 route is unchanged and already known.
 * Independent OC -> OST2 physical source and active level are NOT known.
 */
#ifndef AM13E_BOARD_OC_GPIO_PINCM
#define AM13E_BOARD_OC_GPIO_PINCM 0
#endif
#ifndef AM13E_BOARD_OC_ACTIVE_LOW
#define AM13E_BOARD_OC_ACTIVE_LOW (-1)
#endif
