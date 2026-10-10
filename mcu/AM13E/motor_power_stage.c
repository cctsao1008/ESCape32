/* AM13E reference PB13 Gate Enable: IO INITIALIZATION ONLY.
 *
 * PB13 is driven to the configured INACTIVE level before its GPIO
 * output driver is enabled, then stays inactive. No arming/driver-enable
 * transitions, pinmux attach, or OC Trip routing are implemented.
 *
 * The actual MCPWM0 six-phase PWM and RED/FED software pipeline lives
 * in motor_safety.c; its physical pads remain disconnected as GPIO
 * inputs. This source is deliberately NOT a working motor power stage.
 */
#include "motor_power_stage.h"
#include "board_configuration.h"
#include <soc.h>
#include <dl_gpio.h>
#include <stdint.h>

#define GATE_EN_PIN DL_GPIO_PIN(13U)

_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==0 ||
               AM13E_BOARD_PB13_ACTIVE_LEVEL==1,
               "Reference gate enable inactive level must be binary");

static volatile uint32_t initialized;

static void gate_hold_inactive(void)
{
#if AM13E_BOARD_PB13_ACTIVE_LEVEL == 1
    DL_GPIO_clearPins(GPIO1,GATE_EN_PIN);
#else
    DL_GPIO_setPins(GPIO1,GATE_EN_PIN);
#endif
}

void am13e_power_stage_init(void)
{
    if (__get_PRIMASK()!=1U || initialized)
        for(;;){__NOP();}
    /* GPIO1 is already powered by initgpio(). Do not reset PB14/15. */
    DL_GPIO_enablePower(GPIO1);
    if (!DL_GPIO_isPowerEnabled(GPIO1))
        for(;;){__NOP();}

    gate_hold_inactive(); /* Load inactive DATA before enabling output. */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB13);
    DL_GPIO_enableOutput(GPIO1,GATE_EN_PIN);
    __DSB();
    if ((GPIO1->DOE31_0&GATE_EN_PIN)==0U ||
        (((GPIO1->DOUT31_0&GATE_EN_PIN)!=0U)==
         (AM13E_BOARD_PB13_ACTIVE_LEVEL!=0)))
        for(;;){__NOP();}
    initialized=1U;
}

void am13e_power_stage_force_off(void)
{
    /* Idempotent inactive-only write; never changes PB13 to ACTIVE. */
    if (initialized) {
        gate_hold_inactive();
        __DSB();
    }
}
