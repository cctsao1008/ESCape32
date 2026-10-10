/* MCPWM0 software-side inactive preflight diagnostics.
 * NOTE: This is NOT the power-stage ready/arm/trip-verified state.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only MCPWM0 safety diagnostics"
#endif
#include <stdint.h>
#include "motor_shadow_plan.h"
uint32_t am13e_app_motor_inactive_preflight_ok(void);
/* Shadow-only stage with real register readback; this never arms a motor. */
int am13e_app_motor_stage_inactive_shadow(const AM13E_MotorShadowPlan *plan);
/* Strong vector: physical Trip Zone fault or unexpected MCPWM IRQ. */
void MCPWM0_IRQHandler(void);
/* Debug snapshots: no assertion of a validated hardware trip route. */
void am13e_app_motor_trip_snapshot(uint32_t *irq, uint32_t *tz);
