/* Electrical realization of original Core AQ/compare intent; no motor policy. */
#pragma once
#include "board_configuration.h"
#if AM13E_MOTOR_OUTPUT_CONFIGURED
#include <stdint.h>
void am13e_motor_output_update_begin(void);
void am13e_motor_output_stage(const uint16_t aq[6],const uint16_t compare[6],uint16_t period);
int am13e_motor_output_start(void);
void am13e_motor_output_cancel(void);
int am13e_motor_output_zero_irq(void);
#endif
