/*
 * Required board-supplied six-pad route; NO weak generic fallback.
 * A different AM13E board must explicitly provide its own implementation.
 * The existing full FW1 currently links the Reference provider.
 */
#pragma once
#ifndef AM13E
#error "AM13E board pad route only"
#endif
#include "motor_output_backend.h"
const AM13E_MotorPadRoute *am13e_board_motor_pad_route(void);
