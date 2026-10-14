#pragma once
#include <stdint.h>
/* Exact Rel17 calibirq policy; clock actuation belongs to a qualified
 * board provider, not an STM32 trim unit copied onto AM13E. */
typedef struct {
    int16_t error, high, low;
    uint8_t count, finished;
} AM13E_ServoCalibration;
enum { AM13E_SERVO_CAL_DONE, AM13E_SERVO_CAL_WAIT,
       AM13E_SERVO_CAL_SLOW, AM13E_SERVO_CAL_FAST };
int am13e_servo_calibration_step(AM13E_ServoCalibration *state,
                                  unsigned period_ticks, int enabled);
