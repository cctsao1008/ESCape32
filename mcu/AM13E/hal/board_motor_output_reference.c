/* Selected TI v1.2 Option 1 motor routing (not electrical qualification).
 * PA8/PA21, PA24/PA2, PA10/PA25 are the selected MCPWM4 map.
 * These values do NOT authorize output without gate/Trip/RED/FED review.
 */
#include "board_motor_output_provider.h"
#include "board_reference_io.h"

static const AM13E_MotorPadRoute reference_route = {
    .gpio = AM13E_MOTOR_GPIO,
    .gpio_mask = DL_GPIO_PIN(AM13E_IO_PWM_UH_PINCM) | DL_GPIO_PIN(AM13E_IO_PWM_UL_PINCM) |
                 DL_GPIO_PIN(AM13E_IO_PWM_VH_PINCM) | DL_GPIO_PIN(AM13E_IO_PWM_VL_PINCM) |
                 DL_GPIO_PIN(AM13E_IO_PWM_WH_PINCM) | DL_GPIO_PIN(AM13E_IO_PWM_WL_PINCM),
    .pin_bits = {
        DL_GPIO_PIN(AM13E_IO_PWM_UH_PINCM), DL_GPIO_PIN(AM13E_IO_PWM_UL_PINCM), DL_GPIO_PIN(AM13E_IO_PWM_VH_PINCM),
        DL_GPIO_PIN(AM13E_IO_PWM_VL_PINCM), DL_GPIO_PIN(AM13E_IO_PWM_WH_PINCM), DL_GPIO_PIN(AM13E_IO_PWM_WL_PINCM)
    },
    .pincm = {
        AM13E_IO_PWM_UH_PINCM, AM13E_IO_PWM_UL_PINCM,
        AM13E_IO_PWM_VH_PINCM, AM13E_IO_PWM_VL_PINCM,
        AM13E_IO_PWM_WH_PINCM, AM13E_IO_PWM_WL_PINCM
    },
    .gpio_functions = {
        IOMUX_PA8_GPIO08, IOMUX_PA21_GPIO21,
        IOMUX_PA24_GPIO24, IOMUX_PA2_GPIO02,
        IOMUX_PA10_GPIO10, IOMUX_PA25_GPIO25
    },
    .pwm_functions = {
        IOMUX_PA8_MCPWM4_1A, IOMUX_PA21_MCPWM4_1B,
        IOMUX_PA24_MCPWM4_2A, IOMUX_PA2_MCPWM4_2B,
        IOMUX_PA10_MCPWM4_3A, IOMUX_PA25_MCPWM4_3B
    }
};

const AM13E_MotorPadRoute *am13e_board_motor_pad_route(void)
{
    return &reference_route;
}
