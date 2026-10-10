/*
 * Board-neutral validation of exactly six AM13E MCPWM pad entries.
 * Pure C, no TI HAL or gate-state assumptions.
 */
#pragma once
#include <stdint.h>
#define AM13E_MOTOR_PAD_COUNT 6U
int am13e_motor_pad_route_plan_valid(
    const uint32_t pins[AM13E_MOTOR_PAD_COUNT],
    const uint32_t pincm[AM13E_MOTOR_PAD_COUNT],
    uint32_t gpio_mask);
