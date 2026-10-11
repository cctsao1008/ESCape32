/* Reference-only DShot command wire PB14/GPIO46 -> ECAP0.
 * This is not the universal AM13E MCU IO or power-stage profile.
 * The external bidirectional electrical interface is unqualified.
 */
#pragma once
#ifndef AM13E
#error "Reference command input is AM13E-only"
#endif
#include <soc.h>
#include <dl_gpio.h>
#include <dl_xbar.h>
#define PB14_GPIO        GPIO1
#define PB14_GPIO_PIN    DL_GPIO_PIN(14U)
#define PB14_PINCM       IOMUX_PINCM_PB14
#define PB14_GPIO_NUMBER 46U
#define PB14_ECAP       ECAP0
#define PB14_INPUT_XBAR DL_XBAR_INPUT1
_Static_assert(IOMUX_PINCM_PB14 == 46U, "Reference PB14/GPIO46 route changed");
