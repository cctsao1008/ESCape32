/* Numeric STM32G431-derived defaults and override test.
 * No board electrical qualification or gate output is authorized.
 */
#include "board_configuration.h"
#include <stdio.h>
#if defined(AM13E_BOARD_POWER_STAGE_PROFILE) || \
    defined(AM13E_MOTOR_BOARD_DEADBAND_VERIFIED) || \
    defined(AM13E_BOARD_SENSORS_CALIBRATED)
#error "Numeric development defaults must not authorize output"
#endif
#ifndef AM13E_BOARD_DEFAULTS_OVERRIDE_TEST
enum { G431_DEAD_TIME=155, G431_TIMER_MHZ=168, AM13E_TIMER_MHZ=100,
       G431_DEAD_CYCLES=2*(64+((G431_DEAD_TIME-128)/2)) };
_Static_assert(G431_DEAD_CYCLES==154, "G431 DTG encoding drift");
_Static_assert((G431_DEAD_CYCLES*AM13E_TIMER_MHZ+G431_TIMER_MHZ/2)/G431_TIMER_MHZ==92,
               "G431 to AM13E dead-time conversion drift");
_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==1 &&
               AM13E_BOARD_GATE_PWM_INVERT_MASK==0 &&
               AM13E_BOARD_GATE_INPUTS_HIZ_SAFE==0 &&
               AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN==0,
               "G431-inspired gate defaults must remain unqualified");
_Static_assert(AM13E_MOTOR_DB_RED_TICKS==92 &&
               AM13E_MOTOR_DB_FED_TICKS==92 &&
               AM13E_MOTOR_DB_RED_POLARITY==0 &&
               AM13E_MOTOR_DB_FED_POLARITY==1 &&
               AM13E_MOTOR_DB_RED_INPUT==0 &&
               AM13E_MOTOR_DB_FED_INPUT==0 &&
               AM13E_MOTOR_DB_SWAP_A==0 &&
               AM13E_MOTOR_DB_SWAP_B==0 &&
               AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS==92,
               "G431-derived AM13E dead-band defaults drift");
_Static_assert(AM13E_BOARD_ADC_FULLSCALE==4095 &&
               AM13E_BOARD_ADC_VREF_MV==3300 &&
               AM13E_BOARD_NTC_SUPPLY_MV==3300 &&
               AM13E_BOARD_VBUS_TOP_OHMS==26820 &&
               AM13E_BOARD_VBUS_BOTTOM_OHMS==1000 &&
               AM13E_BOARD_NTC_MODEL==3,
               "G431-inspired sensor model drift");
_Static_assert(AM13E_BOARD_OC_GPIO_PINCM==0 &&
               AM13E_BOARD_OC_ACTIVE_LOW==1,
               "Independent overcurrent should stay disabled");
#else
_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==0 &&
               AM13E_MOTOR_DB_RED_TICKS==64 &&
               AM13E_BOARD_ADC_VREF_MV==3000 &&
               AM13E_BOARD_OC_GPIO_PINCM==49,
               "Reviewed Board Profile must override its chosen values");
_Static_assert(AM13E_BOARD_GATE_PWM_INVERT_MASK==0 &&
               AM13E_BOARD_NTC_MODEL==3 &&
               AM13E_BOARD_OC_ACTIVE_LOW==1,
               "Partial override must preserve other numeric defaults");
#endif
int main(void){
    puts("PASS: numeric G431-derived AM13E default/override without arming");
    return 0;
}
