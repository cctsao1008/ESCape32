#include "motor_pad_backend.h"
#include <stdint.h>
#include <dl_gpio.h>

int am13e_mcu_motor_pad_route_valid(const AM13E_MotorPadRoute *route)
{
    return route != NULL && route->gpio != NULL &&
        am13e_motor_pad_route_plan_valid(route->pin_bits, route->pincm,
                                          route->gpio_mask);
}

int am13e_mcu_motor_pads_gpio_oe_off(const AM13E_MotorPadRoute *route)
{
    return am13e_mcu_motor_pad_route_valid(route) &&
        (route->gpio->DOE31_0 & route->gpio_mask) == 0U;
}

int am13e_mcu_motor_pads_disconnect(const AM13E_MotorPadRoute *route)
{
    if (!am13e_mcu_motor_pad_route_valid(route)) return 0;
    DL_GPIO_enablePower(route->gpio);
    if (!DL_GPIO_isPowerEnabled(route->gpio)) return 0;
    DL_GPIO_disableOutput(route->gpio, route->gpio_mask);
    for (unsigned i=0U;i<AM13E_MOTOR_PAD_COUNT;++i)
        DL_GPIO_initDigitalInput(route->pincm[i]);
    return 1;
}

int am13e_mcu_motor_pads_disconnected(const AM13E_MotorPadRoute *route)
{
    if (!am13e_mcu_motor_pads_gpio_oe_off(route)) return 0;
    for (unsigned i=0U;i<AM13E_MOTOR_PAD_COUNT;++i)
        if (!DL_GPIO_isInputEnabled(route->pincm[i]) ||
            DL_GPIO_getPeripheralFunctionBits(route->pincm[i]) !=
                route->gpio_functions[i]) return 0;
    return 1;
}

int am13e_mcu_motor_pads_connect_pwm(const AM13E_MotorPadRoute *route,
                                      uint32_t invert_mask)
{
    if (!am13e_mcu_motor_pad_route_valid(route) ||
        (invert_mask & ~UINT32_C(0x3f)) != 0U ||
        __get_PRIMASK() != 1U) return 0;
    for (unsigned i=0U;i<AM13E_MOTOR_PAD_COUNT;++i) {
        const DL_GPIO_INVERSION inversion =
            (invert_mask & (UINT32_C(1)<<i)) ?
            DL_GPIO_INVERSION_ENABLE : DL_GPIO_INVERSION_DISABLE;
        DL_GPIO_initPeripheralOutputFunctionFeatures(
            route->pincm[i], route->pwm_functions[i], inversion,
            DL_GPIO_RESISTOR_NONE, DL_GPIO_DRIVE_STRENGTH_LOW,
            DL_GPIO_HIZ_DISABLE);
    }
    return 1;
}

int am13e_mcu_motor_pads_pwm_matches(const AM13E_MotorPadRoute *route,
                                      uint32_t invert_mask)
{
    if (!am13e_mcu_motor_pads_gpio_oe_off(route) ||
        (invert_mask & ~UINT32_C(0x3f)) != 0U) return 0;
    for (unsigned i=0U;i<AM13E_MOTOR_PAD_COUNT;++i) {
        const uint32_t actual =
            IOMUX->SECCFG.PINCM[route->pincm[i]] & IOMUX_PINCM_INV_MASK;
        const uint32_t expected =
            (invert_mask & (UINT32_C(1)<<i)) ?
            IOMUX_PINCM_INV_ENABLE : IOMUX_PINCM_INV_DISABLE;
        if (DL_GPIO_getPeripheralFunctionBits(route->pincm[i]) !=
                route->pwm_functions[i] ||
            actual != expected ||
            !DL_GPIO_isPeripheralConnected(route->pincm[i])) return 0;
    }
    return 1;
}
