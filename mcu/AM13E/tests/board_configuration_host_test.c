#include "board_configuration.h"
#include <stdio.h>
#if defined(AM13E_BOARD_POWER_STAGE_PROFILE) || \
    defined(AM13E_MOTOR_BOARD_DEADBAND_VERIFIED) || \
    defined(AM13E_BOARD_SENSORS_CALIBRATED)
#error "Default board fields must never authorize physical motor output"
#endif
#ifndef AM13E_BOARD_DEFAULTS_OVERRIDE_TEST
_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==-1 &&
               AM13E_BOARD_GATE_PWM_INVERT_MASK==-1 &&
               AM13E_BOARD_GATE_INPUTS_HIZ_SAFE==0 &&
               AM13E_BOARD_GATE_DRIVER_HAS_HW_SHUTDOWN==0,
               "Do not guess gate-driver polarity, bias or safety");
_Static_assert(AM13E_MOTOR_DB_RED_TICKS==0 &&
               AM13E_MOTOR_DB_FED_TICKS==0 &&
               AM13E_MOTOR_DB_RED_INPUT==-1 &&
               AM13E_MOTOR_DB_FED_INPUT==-1 &&
               AM13E_MOTOR_DB_SWAP_A==-1 &&
               AM13E_MOTOR_DB_SWAP_B==-1,
               "Do not guess dead-band topology or delays");
_Static_assert(AM13E_BOARD_ADC_FULLSCALE==4095 &&
               AM13E_BOARD_ADC_VREF_MV==0 &&
               AM13E_BOARD_NTC_SUPPLY_MV==0 &&
               AM13E_BOARD_VBUS_TOP_OHMS==0 &&
               AM13E_BOARD_VBUS_BOTTOM_OHMS==0 &&
               AM13E_BOARD_NTC_MODEL==0,
               "Uncalibrated analog values must remain unknown");
_Static_assert(AM13E_BOARD_OC_GPIO_PINCM==0 &&
               AM13E_BOARD_OC_ACTIVE_LOW==-1,
               "OC input cannot be invented");
#else
_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==1 &&
               AM13E_MOTOR_DB_RED_TICKS==43 &&
               AM13E_BOARD_ADC_VREF_MV==3100 &&
               AM13E_BOARD_OC_GPIO_PINCM==49,
               "Explicit board overrides must supersede defaults");
_Static_assert(AM13E_BOARD_OC_ACTIVE_LOW==-1 &&
               AM13E_BOARD_NTC_MODEL==0,
               "Unknown values must stay unknown after partial override");
#endif
int main(void) {
    puts("PASS: board defaults/overrides require explicit physical qualification");
    return 0;
}
