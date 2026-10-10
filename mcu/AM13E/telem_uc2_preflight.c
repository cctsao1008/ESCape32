/* ESCape32 Rel17 UART: AM13E UC2 power, clock and register preflight.
 *
 * TI SDK: dl_unicomm.h, dl_unicommuart.h, dl_gpio.h, clock backend.
 * Board mapping: PA22 UC2 TX, PA23 UC2 RX (PinMux function 4).
 * Initial physical pad mode is GPIO input on BOTH pins (MCU Hi-Z).
 * DO NOT connect to a single-wire RC bus until PHY direction/level and
 * possible pin inversion are qualified on the product board.
 *
 * This is not a substitute for the five missing am13e_telem_hw_* APIs.
 * RX framing, EOT-based TX completion, buffer lifetime, protocol reply,
 * S.Port 26-bit gap and single-wire direction remain to be implemented.
 */
#include "telem_uc2_preflight.h"
#include "clock_backend.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_unicomm.h>
#include <dl_unicommuart.h>
#include <dl_sysctl.h>

#define UC2_BUSCLK_HZ (AM13E_APP_MCLK_HZ / 2U)
#define TELEMETRY_PADS (DL_GPIO_PIN(22U) | DL_GPIO_PIN(23U))
_Static_assert(IOMUX_PINCM_PA22==22U && IOMUX_PINCM_PA23==23U,
               "UART telemetry pins changed");
_Static_assert(IOMUX_PA22_UC2_TX_SDA==4U && IOMUX_PA23_UC2_RX_SCL==4U,
               "UC2 UART peripheral mux assignment changed");
_Static_assert(UC2_BUSCLK_HZ == 100000000U,
               "UC2 peripheral BUSCLK contract changed");
static volatile uint32_t uc2_prepared;

int am13e_uc2_uart_preflight(uint32_t baud, uint32_t timeout_field)
{
    /* Only initial FW1 bring-up, before global IRQ unmask. */
    if (uc2_prepared || __get_PRIMASK()==0U ||
        (SYSCTL->SOCLOCK.MCLKCFG & SYSCTL_MCLKCFG_MCLKDIVCFG_MASK) !=
          (uint32_t)DL_SYSCTL_MCLK_DIV_2_DIV_4) return 0;

    /* A value above 15 is not representable in UC2 RXTOSEL;
     * S.Port 26 MUST acquire a real alternative before being enabled.
     */
    if (timeout_field > 15U || baud < 19200U || baud > 416666U)
        return 0;

    DL_GPIO_enablePower(GPIO0);
    if (!DL_GPIO_isPowerEnabled(GPIO0)) return 0;
    DL_GPIO_disableOutput(GPIO0, TELEMETRY_PADS);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA22);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA23);

    DL_UART_enablePower(UC2_INST_PTR);
    if (!DL_UART_isPowerEnabled(UC2_INST_PTR)) return 0;
    if (DL_UART_isEnabled(UC2_INST_PTR)) return 0;

    DL_UART_Config cfg = {
        .mode=DL_UART_MODE_NORMAL,
        .direction=DL_UART_DIRECTION_NONE,
        .flowControl=DL_UART_FLOW_CONTROL_NONE,
        .parity=DL_UART_PARITY_NONE,
        .wordLength=DL_UART_WORD_LENGTH_8_BITS,
        .stopBits=DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(UC2_INST_PTR, &cfg);
    DL_UART_ClockConfig clock = {
        .clockSel=DL_UART_CLOCK_BUSCLK,
        .divideRatio=DL_UART_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_setClockConfig(UC2_INST_PTR, &clock);
    DL_UART_configBaudRate(UC2_INST_PTR, UC2_BUSCLK_HZ, baud);
    DL_UART_setRXInterruptTimeout(UC2_INST_PTR, timeout_field);

    /* Readback proves register/configuration state only. */
    if (!DL_GPIO_isInputEnabled(IOMUX_PINCM_PA22) ||
        !DL_GPIO_isInputEnabled(IOMUX_PINCM_PA23) ||
        DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA22) !=
          IOMUX_PA22_GPIO22 ||
        DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA23) !=
          IOMUX_PA23_GPIO23 ||
        (GPIO0->DOE31_0 & TELEMETRY_PADS) != 0U ||
        DL_UART_getRXInterruptTimeout(UC2_INST_PTR) != timeout_field ||
        DL_UART_getDirection(UC2_INST_PTR) != DL_UART_DIRECTION_NONE ||
        DL_UART_getIntegerBaudRateDivisor(UC2_INST_PTR)==0U)
        return 0;

    uc2_prepared=1U;
    return 1;
}

int am13e_uc2_uart_is_prepared(void)
{
    return uc2_prepared != 0U &&
           DL_UART_isPowerEnabled(UC2_INST_PTR) &&
           DL_UART_getDirection(UC2_INST_PTR) == DL_UART_DIRECTION_NONE &&
           (GPIO0->DOE31_0 & TELEMETRY_PADS) == 0U;
}
