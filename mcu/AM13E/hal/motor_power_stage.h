/* PB13 gate lifecycle: inactive at reset/stop/fault; enabled only by the
 * original Core's motor/audio request and native carrier startup.
 * Board electrical definitions are required; Reference guesses do not
 * select the physical output path. */
#pragma once
#ifndef AM13E
#error "AM13E reference gate initialization only"
#endif
void am13e_power_stage_init(void);
void am13e_power_stage_force_off(void);

#include "board_configuration.h"
#if AM13E_MOTOR_OUTPUT_CONFIGURED
int am13e_power_stage_enable(void);
int am13e_power_stage_inactive(void);
#endif
