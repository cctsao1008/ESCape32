/*
** ESCape32 motor hardware contract.
**
** Keep this interface semantic: it describes motor-control operations required
** by ESCape32, not STM32 timer registers. Backends are selected at compile
** time so the commutation hot path does not pay function-call overhead.
*/

#pragma once

#ifdef ESCAPE32_AM13E
#include "hw_motor_am13e.h"
#else
#include "hw_motor_libopencm3.h"
#endif
