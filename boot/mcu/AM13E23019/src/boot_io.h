#pragma once
#include <stdbool.h>

/* Shared Boot policy for FW1 (six-step) and FW2 (FOC).
 * Returns false if the safe GPIO baseline cannot be established.
 */
bool boot_io_init_safe_state(void);
void boot_io_prepare_app_handoff(void);
