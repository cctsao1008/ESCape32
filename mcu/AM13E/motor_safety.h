/* MCPWM0 software-side inactive preflight diagnostics.
 * NOTE: This is NOT the power-stage ready/arm/trip-verified state.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only MCPWM0 safety diagnostics"
#endif
#include <stdint.h>
uint32_t am13e_app_motor_inactive_preflight_ok(void);
