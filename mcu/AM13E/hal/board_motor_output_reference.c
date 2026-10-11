/* Existing Reference v1.6 motor pad routing only (not a universal BSP).
 * PA8/PA11, PA9/PA30, PA10/PA31 remain the exact old MCPWM0 map.
 * These values do NOT authorize output without gate/Trip/RED/FED review.
 */
#include "board_motor_output_provider.h"
#include "board_reference_io.h"

static const AM13E_MotorPadRoute reference_route = {
    .gpio = GPIO0,
    .gpio_mask = DL_GPIO_PIN(8U) | DL_GPIO_PIN(11U) |
                 DL_GPIO_PIN(9U) | DL_GPIO_PIN(30U) |
                 DL_GPIO_PIN(10U) | DL_GPIO_PIN(31U),
    .pin_bits = {
        DL_GPIO_PIN(8U), DL_GPIO_PIN(11U), DL_GPIO_PIN(9U),
        DL_GPIO_PIN(30U), DL_GPIO_PIN(10U), DL_GPIO_PIN(31U)
    },
    .pincm = {
        AM13E_IO_PWM_UH_PINCM, AM13E_IO_PWM_UL_PINCM,
        AM13E_IO_PWM_VH_PINCM, AM13E_IO_PWM_VL_PINCM,
        AM13E_IO_PWM_WH_PINCM, AM13E_IO_PWM_WL_PINCM
    },
    .gpio_functions = {
        IOMUX_PA8_GPIO08, IOMUX_PA11_GPIO11,
        IOMUX_PA9_GPIO09, IOMUX_PA30_GPIO30,
        IOMUX_PA10_GPIO10, IOMUX_PA31_GPIO31
    },
    .pwm_functions = {
        IOMUX_PA8_MCPWM0_1A, IOMUX_PA11_MCPWM0_1B,
        IOMUX_PA9_MCPWM0_2A, IOMUX_PA30_MCPWM0_2B,
        IOMUX_PA10_MCPWM0_3A, IOMUX_PA31_MCPWM0_3B
    }
};

const AM13E_MotorPadRoute *am13e_board_motor_pad_route(void)
{
    return &reference_route;
}
