#include "command_input_route_backend.h"
#include <stddef.h>

int am13e_mcu_command_input_configure(const AM13E_CommandInputRoute *route)
{
    if (route == NULL || route->gpio == NULL ||
        route->pin_mask == 0U ||
        (route->pin_mask & (route->pin_mask - 1U)) != 0U ||
        route->gpio_index >= 107U ||
        route->pincm >= 107U ||
        route->input_xbar > DL_XBAR_INPUT16 ||
        __get_PRIMASK() != 1U) {
        return 0;
    }
    /* This exact input only. Do not reset its port, set GPIO outputs
     * or enable pull/bias without independent board-level approval.
     */
    DL_GPIO_enablePower(route->gpio);
    if (!DL_GPIO_isPowerEnabled(route->gpio)) return 0;
    DL_GPIO_disableOutput(route->gpio, route->pin_mask);
    DL_GPIO_initDigitalInput(route->pincm);
    if (!DL_GPIO_isInputEnabled(route->pincm) ||
        !DL_GPIO_isPeripheralConnected(route->pincm) ||
        DL_GPIO_getPeripheralFunctionBits(route->pincm) !=
            route->gpio_function) {
        return 0;
    }
    DL_GPIO_disableInterrupt(route->gpio, route->pin_mask);
    DL_GPIO_clearInterruptStatus(route->gpio, route->pin_mask);
    DL_XBAR_setInputXBAR(route->input_xbar, route->gpio_index);
    return INPUTXBAR->INPUTSELECT[(uint32_t)route->input_xbar] ==
           route->gpio_index;
}
