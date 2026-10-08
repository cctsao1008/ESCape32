/*
 * E62 common Boot-safe GPIO policy.
 * Pin allocation: E62 HW Architecture Baseline v1.6, section 14.
 *
 * IMPORTANT: A high-Z MCU output does not guarantee an external gate driver is
 * disabled. The PB13 enable polarity and external biasing are not finalized.
 * Hardware must implement a default-off power-stage state independent of MCU.
 * Do not assert PB13 or enable MCPWM outputs in Boot.
 */
#include "boot_io.h"
#include "dl_gpio.h"
#include "dl_common.h"
#include "soc.h"

#define E62_PWM_GPIO_MASK ((1UL << 8) | (1UL << 9) | (1UL << 10) | \
                           (1UL << 11) | (1UL << 30) | (1UL << 31))
#define E62_GATE_ENABLE_MASK (1UL << 13)

static void e62_input_no_pull(uint32_t pin)
{
    DL_GPIO_initDigitalInputFeatures(
        pin, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_WAKEUP_DISABLE);
}

bool boot_io_init_safe_state(void)
{
    /* GPIO ports A/B. Do not touch HFXT, SWD, sensing or reserved pins. */
    DL_GPIO_enablePower(GPIO0);
    DL_GPIO_enablePower(GPIO1);
    DL_Common_delayCycles(16U);

    /* Remove GPIO output drive BEFORE changing pin mux. */
    DL_GPIO_disableOutput(GPIO0, E62_PWM_GPIO_MASK);
    DL_GPIO_disableOutput(GPIO1, E62_GATE_ENABLE_MASK);

    e62_input_no_pull(IOMUX_PINCM_PA8);   /* PWM_UH */
    e62_input_no_pull(IOMUX_PINCM_PA11);  /* PWM_UL */
    e62_input_no_pull(IOMUX_PINCM_PA9);   /* PWM_VH */
    e62_input_no_pull(IOMUX_PINCM_PA30);  /* PWM_VL */
    e62_input_no_pull(IOMUX_PINCM_PA10);  /* PWM_WH */
    e62_input_no_pull(IOMUX_PINCM_PA31);  /* PWM_WL */
    e62_input_no_pull(IOMUX_PINCM_PB13);  /* Gate enable, no assumed polarity */

    /* PB15 nFAULT intentionally left to application ownership.
     * PB14 service transport initialized separately.
     */
    return true;
}

void boot_io_prepare_app_handoff(void)
{
    /* Release PB14 as high-impedance input; FW1/FW2 own its runtime mux.
     * Software UART should already be idle when this function is called.
     */
    DL_GPIO_disableOutput(GPIO1, 1UL << 14);
    e62_input_no_pull(IOMUX_PINCM_PB14);
}
