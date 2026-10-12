/* Development command wire PA26/GPIO26 -> TIMG4 CCP0.
 * This is not the universal AM13E MCU IO or power-stage profile.
 * The external bidirectional electrical interface is unqualified.
 */
#pragma once
#ifndef AM13E
#error "Reference command input is AM13E-only"
#endif
#include "../config.h"
#include <soc.h>
#include <dl_gpio.h>
#define COMMAND_GPIO IO_GPIO
#define COMMAND_PIN IO_PIN
#define COMMAND_PINCM IO_PINCM
#define COMMAND_CAPTURE_FUNCTION IOTIM_CAPTURE_FUNCTION
_Static_assert(IOMUX_PINCM_PA26==26U && IOMUX_PA26_TIMG4_0_CCP0==3U &&
    IOMUX_PA26_UC0_RX_SCL_SCLK==8U && IOMUX_PA26_UC0_TX_SDA_PICO==9U &&
    IOMUX_PA26_UC3_TX_SDA_PICO==10U && IOMUX_PA26_UC3_RX_SCL_SCLK==11U,
    "PA26 native command mux differs from datasheet table 5-2");
