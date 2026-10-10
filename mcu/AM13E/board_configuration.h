/* ESCape32 Rel17 STM32G431-derived AM13E numeric development defaults.
 * G431 PHOTONDRIVE1: DEAD_TIME=155, VOLT_MUL=224,
 * TEMP_FUNC=NTC10K3455UP10K, see original CMakeLists.txt.
 *
 * TIM1 DTG(155)=0x8D -> 154 timer cycles @168MHz = 916.7 ns.
 * AM13E MCPWM @100MHz -> 92 ticks = 920ns per RED/FED.
 * VOLT_MUL=224 approximated with 3.3V/12bit nominal ADC model
 * as divider 26820:1000. These are software estimates, not actual
 * AM13E VREF, external resistor values, gate polarity, or measurements.
 *
 * PB13 high=enable is a provisional active-high convention; G431 has
 * no equivalent verified PB13 gate pin on its PHOTONDRIVE1 target.
 * Independent OC is explicitly disabled (PINCM=0), rather than assigned
 * a fabricated pin; the known PB15 nFAULT -> OST1 stays unchanged.
 *
 * 0 for Hi-Z safety and shutdown proof means NOT HARDWARE VERIFIED.
 * By default Reference FW1 now compiles live motor/ADC paths via CMake;
 * numeric values are development settings, not real PCB sign-off.
 * All entries remain overridable via -D or CMake.
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

/* Derived PHOTONDRIVE1 VOLT_MUL=224 model, not actual AM13E sensing:
 * 3300mV, 12-bit ADC and 26820/1000ohm divider approximate G431
 * voltage gain; NTC model3 = G431 NTC10K3455UP10K.
 */
#ifndef AM13E_BOARD_ADC_FULLSCALE
#define AM13E_BOARD_ADC_FULLSCALE 4095
#endif
#ifndef AM13E_BOARD_ADC_VREF_MV
#define AM13E_BOARD_ADC_VREF_MV 3300
#endif
#ifndef AM13E_BOARD_NTC_SUPPLY_MV
#define AM13E_BOARD_NTC_SUPPLY_MV 3300
#endif
#ifndef AM13E_BOARD_VBUS_TOP_OHMS
#define AM13E_BOARD_VBUS_TOP_OHMS 26820
#endif
#ifndef AM13E_BOARD_VBUS_BOTTOM_OHMS
#define AM13E_BOARD_VBUS_BOTTOM_OHMS 1000
#endif
#ifndef AM13E_BOARD_NTC_MODEL
#define AM13E_BOARD_NTC_MODEL 3
#endif

/* PB15 nFAULT -> OST1 is always installed/checked at runtime.
 * Independent OC -> OST2 is an OPTIONAL board path (PINCM=0 disables
 * only the second trip). COMP_MAP=132 is G431 BEMF, not actual OC.
 */
#ifndef AM13E_BOARD_OC_GPIO_PINCM
#define AM13E_BOARD_OC_GPIO_PINCM 0
#endif
#ifndef AM13E_BOARD_OC_ACTIVE_LOW
#define AM13E_BOARD_OC_ACTIVE_LOW 1
#endif
