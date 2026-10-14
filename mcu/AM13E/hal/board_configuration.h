/* Selected Option 1 board contracts. Retained G431-derived internal
 * dead-band arithmetic is an inactive development model, not a driver
 * specification. Physical outputs, overcurrent and sensor calibration
 * require explicit board inputs; the selected Reference is unqualified. */
#pragma once
#ifndef AM13E
#error "AM13E board configuration is not for legacy MCU builds"
#endif

#include "motor_output_configuration.h"
/* Existing six motor pads, PB13/PA26/PB15 and ADC pin routing stay in
 * board_reference_io.h, board_motor_output_reference.c,
 * command_input_reference.h, motor_fault_route.c and analog_reference.h.
 */
#ifndef AM13E_BOARD_PB13_ACTIVE_LEVEL
#define AM13E_BOARD_PB13_ACTIVE_LEVEL 1
#endif
#ifndef AM13E_BOARD_GATE_PWM_INVERT_MASK
#define AM13E_BOARD_GATE_PWM_INVERT_MASK 0
#endif
#ifndef AM13E_BOARD_GATE_INPUTS_HIZ_SAFE
#define AM13E_BOARD_GATE_INPUTS_HIZ_SAFE 0
#endif
#ifndef AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN
#define AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN 0
#endif

/* G431 encoded 916.7ns -> 92 AM13E MCPWM clocks at 100MHz.
 * Conventional PWM A RED/FED inputs 0, RED polarity 0, FED 1;
 * this is a development topology, not a verified electrical path.
 */
#ifndef AM13E_MOTOR_DB_RED_TICKS
#define AM13E_MOTOR_DB_RED_TICKS 92
#endif
#ifndef AM13E_MOTOR_DB_FED_TICKS
#define AM13E_MOTOR_DB_FED_TICKS 92
#endif
#ifndef AM13E_MOTOR_DB_RED_POLARITY
#define AM13E_MOTOR_DB_RED_POLARITY 0
#endif
#ifndef AM13E_MOTOR_DB_FED_POLARITY
#define AM13E_MOTOR_DB_FED_POLARITY 1
#endif
#ifndef AM13E_MOTOR_DB_RED_INPUT
#define AM13E_MOTOR_DB_RED_INPUT 0
#endif
#ifndef AM13E_MOTOR_DB_FED_INPUT
#define AM13E_MOTOR_DB_FED_INPUT 0
#endif
#ifndef AM13E_MOTOR_DB_SWAP_A
#define AM13E_MOTOR_DB_SWAP_A 0
#endif
#ifndef AM13E_MOTOR_DB_SWAP_B
#define AM13E_MOTOR_DB_SWAP_B 0
#endif
#ifndef AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS
#define AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS 92
#endif

/* Option 1 supplies no sensor calibration. An explicit board profile
 * may define AM13E_BOARD_SENSORS_CONFIGURED and its complete numeric
 * transfer model; there is no G431 calibration fallback. */

/* Optional auxiliaries remain separately reserved. Option 1 phase OC
 * uses CMPSS1/2/3; PA22 nFAULT uses the dedicated OST2 route. */
#ifndef AM13E_BOARD_OC_GPIO_PINCM
#define AM13E_BOARD_OC_GPIO_PINCM 0
#endif
#ifndef AM13E_BOARD_OC_ACTIVE_LOW
#define AM13E_BOARD_OC_ACTIVE_LOW 1
#endif
/* Unassigned auxiliary pins: 0 = no pin, NO peripheral traffic.
 * When explicitly assigned: only GPIO Input or analog pinmux is set.
 * No Serial protocol, hardware OC Trip or current-limit algorithm.
 */
#ifndef AM13E_BOARD_SERIAL_TX_PINCM
#define AM13E_BOARD_SERIAL_TX_PINCM 0
#endif
#ifndef AM13E_BOARD_CURRENT_SENSE_PINCM
#define AM13E_BOARD_CURRENT_SENSE_PINCM 0
#endif


#include "board_pin_ownership.h"
