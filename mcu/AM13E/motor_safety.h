/* MCPWM0 software-side inactive preflight diagnostics.
 * NOTE: This is NOT the power-stage ready/arm/trip-verified state.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only MCPWM0 safety diagnostics"
#endif
#include <stdint.h>
uint32_t am13e_app_motor_inactive_preflight_ok(void);
/* Strong vector: physical Trip Zone fault or unexpected MCPWM IRQ. */
void MCPWM0_IRQHandler(void);
/* Debug snapshots: no assertion of a validated hardware trip route. */
void am13e_app_motor_trip_snapshot(uint32_t *irq, uint32_t *tz);
