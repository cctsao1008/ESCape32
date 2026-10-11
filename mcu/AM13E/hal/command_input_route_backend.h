/* AM13E MCU GPIO/INPUTXBAR command capture route: no board defaults.
 * Explicitly selected GPIO/PINCM/input XBAR; no pull/bias/voltage
 * assumptions. Does not configure ECAP or enable power output.
 */
#pragma once
#ifndef AM13E
#error "AM13E command capture backend is not for legacy MCU targets"
#endif
#include <stdint.h>
#include <soc.h>
#include <dl_gpio.h>
#include <dl_xbar.h>
typedef struct {
    GPIO_Regs *gpio;
    uint32_t pin_mask;
    uint32_t pincm;
    uint32_t gpio_function;
    uint8_t gpio_index;
    DL_XBAR_InputNum input_xbar;
} AM13E_CommandInputRoute;
/* Requires masked PRIMASK. Zero indicates invalid route/readback. */
int am13e_mcu_command_input_configure(const AM13E_CommandInputRoute *route);
