/*
 * Generic real AM13E six-MCPWM-pad MCU backend.
 * The board supplies physical GPIO/PINCM, GPIO mux and MCPWM mux.
 * No crystal, fault active level, gate output or dead-band defaults.
 */
#pragma once
#ifndef AM13E
#error "AM13E motor pad backend cannot be used on legacy MCUs"
#endif
#include <soc.h>
#include <dl_gpio.h>
#include "motor_output_route_plan.h"

typedef struct {
    GPIO_Regs *gpio;
    uint32_t gpio_mask;
    uint32_t pin_bits[AM13E_MOTOR_PAD_COUNT];
    uint32_t pincm[AM13E_MOTOR_PAD_COUNT];
    uint32_t gpio_functions[AM13E_MOTOR_PAD_COUNT];
    uint32_t pwm_functions[AM13E_MOTOR_PAD_COUNT];
} AM13E_MotorPadRoute;

int am13e_mcu_motor_pad_route_valid(const AM13E_MotorPadRoute *route);

/* Can be called from the fault path regardless of current PRIMASK.
 * Never drives a GPIO high/low or connects a PWM output.
 * Returns 0 on incomplete route or unavailable GPIO power.
 */
int am13e_mcu_motor_pads_disconnect(const AM13E_MotorPadRoute *route);

/* Actual GPIO input and IOMUX-function readback. */
int am13e_mcu_motor_pads_disconnected(const AM13E_MotorPadRoute *route);

/* Requires PRIMASK=1 and an external disabled gate. Caller is solely
 * responsible for OST/dead-band/OC checks and late gate enable.
 * Does not assert any gate pin or modify MCPWM AQ/Trip register.
 */
int am13e_mcu_motor_pads_connect_pwm(const AM13E_MotorPadRoute *route,
                                      uint32_t invert_mask);

/* Read actual PWM alternate functions and inversion (plus GPIO OE off). */
int am13e_mcu_motor_pads_pwm_matches(const AM13E_MotorPadRoute *route,
                                      uint32_t invert_mask);

/* Hardware GPIO output-enable readback only, no pinmux requirement. */
int am13e_mcu_motor_pads_gpio_oe_off(const AM13E_MotorPadRoute *route);
