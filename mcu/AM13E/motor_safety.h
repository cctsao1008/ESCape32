/* MCPWM0 software-side inactive preflight diagnostics.
 * NOTE: This is NOT the power-stage ready/arm/trip-verified state.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only MCPWM0 safety diagnostics"
#endif
#include <stdint.h>
#include "motor_shadow_plan.h"
#include "motor_aq_plan.h"
#include "motor_pwm_shadow_plan.h"
uint32_t am13e_app_motor_inactive_preflight_ok(void);
/* Boot-time register preflight, NOT power stage ready. */
int am13e_app_motor_inactive_aq_boot_preflight(void);
/* Write and read back six candidate AQ shadow registers, with output Hi-Z,
 * AQ shadow FREEZE and TBCLK stopped. Not a power-stage arming function.
 */
int am13e_app_motor_stage_inactive_aq_shadow(const AM13E_MotorAQShadowPlan *plan);
int am13e_app_motor_stage_inactive_sixstep_aq(
    int positive_mask,int negative_mask,int comp_code,int damp,int reverse);
/* Shadow-only stage with real register readback; this never arms a motor. */
int am13e_app_motor_stage_inactive_shadow(const AM13E_MotorShadowPlan *plan);
/* Rel17 frequency and duty -> real inert PWM shadow registers, never arm. */
int am13e_app_motor_stage_inactive_pwm_shadow(
    const AM13E_MotorPwmShadowInputs *input, AM13E_MotorShadowPlan *snapshot);
/* Strong vector: physical Trip Zone fault or unexpected MCPWM IRQ. */
void MCPWM0_IRQHandler(void);
/* Debug snapshots: no assertion of a validated hardware trip route. */
void am13e_app_motor_trip_snapshot(uint32_t *irq, uint32_t *tz);
