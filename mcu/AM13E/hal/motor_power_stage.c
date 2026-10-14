/* PB13 Gate Enable: inactive at init/reset/fault; Rel17 owns runtime enable.
 *
 * PB13 is driven to the configured INACTIVE level before its GPIO
 * output driver is enabled. A complete electrical board configuration
 * permits the original Core motor/audio lifecycle to activate it.
 *
 * The actual MCPWM4 six-phase PWM and RED/FED software pipeline lives
 * in motor_safety.c; its physical pads remain disconnected as GPIO
 * inputs until a board supplies the required electrical definitions.
 */
#include "motor_power_stage.h"
#include "board_configuration.h"
#include <soc.h>
#include <dl_gpio.h>
#include <stdint.h>

#define GATE_EN_PIN AM13E_GATE_MASK

_Static_assert(AM13E_BOARD_PB13_ACTIVE_LEVEL==0 ||
               AM13E_BOARD_PB13_ACTIVE_LEVEL==1,
               "Reference gate enable inactive level must be binary");

static volatile uint32_t initialized;

#if AM13E_MOTOR_OUTPUT_CONFIGURED
static void gate_hold_inactive(void)
{
#if AM13E_BOARD_PB13_ACTIVE_LEVEL == 1
    DL_GPIO_clearPins(AM13E_GATE_GPIO,GATE_EN_PIN);
#else
    DL_GPIO_setPins(AM13E_GATE_GPIO,GATE_EN_PIN);
#endif
}

#endif
void am13e_power_stage_init(void)
{
    if (__get_PRIMASK()!=1U || initialized)
        for(;;){__NOP();}
    /* AM13E_GATE_GPIO is already powered by initgpio(). Do not reset PB14/15. */
    DL_GPIO_enablePower(AM13E_GATE_GPIO);
    if (!DL_GPIO_isPowerEnabled(AM13E_GATE_GPIO))
        for(;;){__NOP();}

#if !AM13E_MOTOR_OUTPUT_CONFIGURED
    /* Option 1 provides no gate polarity/reset circuit. Do not actively
     * drive a guessed inactive level on an unqualified power stage. */
    DL_GPIO_disableOutput(AM13E_GATE_GPIO,GATE_EN_PIN);
    DL_GPIO_initDigitalInput(AM13E_GATE_PINCM);
    initialized=1U;
    return;
#else
    gate_hold_inactive(); /* Explicit inactive DATA before enabling output. */
    DL_GPIO_initDigitalOutput(AM13E_GATE_PINCM);
    DL_GPIO_enableOutput(AM13E_GATE_GPIO,GATE_EN_PIN);
    __DSB();
    if ((AM13E_GATE_GPIO->DOE31_0&GATE_EN_PIN)==0U ||
        (((AM13E_GATE_GPIO->DOUT31_0&GATE_EN_PIN)!=0U)==
         (AM13E_BOARD_PB13_ACTIVE_LEVEL!=0)))
        for(;;){__NOP();}
    initialized=1U;
#endif
}

void am13e_power_stage_force_off(void)
{
    /* Idempotent inactive-only write; never changes PB13 to ACTIVE. */
    if (initialized) {
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        gate_hold_inactive();
#else
        DL_GPIO_disableOutput(AM13E_GATE_GPIO,GATE_EN_PIN);
#endif
        __DSB();
    }
}

#if AM13E_MOTOR_OUTPUT_CONFIGURED
int am13e_power_stage_inactive(void)
{
    return initialized && (((AM13E_GATE_GPIO->DOUT31_0&GATE_EN_PIN)!=0U)!=(AM13E_BOARD_PB13_ACTIVE_LEVEL!=0));
}
int am13e_power_stage_enable(void)
{
    if(!initialized || __get_PRIMASK()!=1U)return 0;
#if AM13E_BOARD_PB13_ACTIVE_LEVEL == 1
    DL_GPIO_setPins(AM13E_GATE_GPIO,GATE_EN_PIN);
#else
    DL_GPIO_clearPins(AM13E_GATE_GPIO,GATE_EN_PIN);
#endif
    __DSB();
    return (AM13E_GATE_GPIO->DOE31_0&GATE_EN_PIN)!=0U &&
        (((AM13E_GATE_GPIO->DOUT31_0&GATE_EN_PIN)!=0U)==(AM13E_BOARD_PB13_ACTIVE_LEVEL!=0));
}
#endif
