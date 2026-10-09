/*
 * ESCape32 Rel17 / AM13E23019 GPIO foundation.
 *
 * The E62 HW Architecture Baseline v1.6 assigns power-stage nFAULT to
 * PB15 / GPIO47. This code configures ONLY that MCU input. It neither
 * assumes PB13 gate-enable polarity nor enables any PWM/power output.
 *
 * The external nFAULT circuit, input bias and electrical behavior remain
 * detailed-HW-design and on-board validation items. Reading an idle-high
 * pin is NOT proof of a functional gate-driver fault path.
 */
#include <soc.h>
#include "gpio_runtime.h"
#include <dl_gpio.h>

#define AM13E_NFAULT_GPIO GPIO1
#define AM13E_NFAULT_PIN DL_GPIO_PIN(15U)
#define AM13E_NFAULT_PINCM IOMUX_PINCM_PB15

_Static_assert(IOMUX_PINCM_PB15 == 47,
               "E62 nFAULT pin mapping must stay PB15 / GPIO47");

static volatile unsigned int nfault_input_initialized;

static void gpio_fail_closed(void)
{
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

void initgpio(void)
{
    /* Boot may own PB14 on this same port. Do not reset all of GPIOB. */
    DL_GPIO_enablePower(AM13E_NFAULT_GPIO);
    if (!DL_GPIO_isPowerEnabled(AM13E_NFAULT_GPIO)) {
        gpio_fail_closed();
    }

    /* Configure only PB15, without asserting or deasserting PB13. */
    DL_GPIO_disableOutput(AM13E_NFAULT_GPIO, AM13E_NFAULT_PIN);
    DL_GPIO_initDigitalInput(AM13E_NFAULT_PINCM);

    if (!DL_GPIO_isInputEnabled(AM13E_NFAULT_PINCM) ||
        !DL_GPIO_isPeripheralConnected(AM13E_NFAULT_PINCM) ||
        DL_GPIO_getPeripheralFunctionBits(AM13E_NFAULT_PINCM) !=
            IOMUX_PB15_GPIO47) {
        gpio_fail_closed();
    }

    nfault_input_initialized = 1U;
}

int am13e_app_nfault_asserted(void)
{
    /* Unknown/uninitialized input is a fault, never a false OK. */
    if (!nfault_input_initialized ||
        !DL_GPIO_isPowerEnabled(AM13E_NFAULT_GPIO) ||
        !DL_GPIO_isInputEnabled(AM13E_NFAULT_PINCM) ||
        !DL_GPIO_isPeripheralConnected(AM13E_NFAULT_PINCM) ||
        DL_GPIO_getPeripheralFunctionBits(AM13E_NFAULT_PINCM) !=
            IOMUX_PB15_GPIO47) {
        return 1;
    }

    /* nFAULT is active low. The physical input must be qualified on-board. */
    return DL_GPIO_readPins(AM13E_NFAULT_GPIO, AM13E_NFAULT_PIN) == 0U;
}
