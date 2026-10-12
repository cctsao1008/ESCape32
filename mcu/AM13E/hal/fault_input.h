/* AM13E reference auxiliary IO-only pin initialization.
 * The active-low PB15 nFAULT net is configured as a bare GPIO input.
 * No nFAULT interrupt, software fault supervision, or MCPWM trip.
 */
#pragma once
#ifndef AM13E
#error "AM13E auxiliary IO initialization only"
#endif
#include "board_io.h"
#define AM13E_APP_NFAULT_PIN_MASK AM13E_NFAULT_MASK
void initgpio(void);
