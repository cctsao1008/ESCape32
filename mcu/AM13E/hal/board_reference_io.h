/*
 * AM13E reference AM13E23019 IO Plan for TI Review v1.0 — PROVISIONAL pin mapping.
 * Source: initial provisional IO mapping, rows 9–25.
 * Pin mapping is user-selected for FW1 porting, NOT electrical sign-off.
 *
 * This file identifies real MCU pad/PINCM and analog comparator selections.
 * It must NOT imply verified PB13 gate-enable polarity, non-overlap/deadtime,
 * MOSFET driver logic, or independent overcurrent MCPWM Trip routing.
 */
#pragma once
#ifndef AM13E
#error "AM13E reference IO Plan is only applicable to AM13E FW1"
#endif
#ifndef AM13E_REF_IO_PLAN_V1
#error "Provisional AM13E reference IO Plan must be explicitly selected by the build"
#elif AM13E_REF_IO_PLAN_V1 != 1
#error "AM13E reference IO Plan selection must equal 1; 0 is not an enabled plan"
#endif
#include "board_io.h"
#include <soc.h>
#include <dl_sysctl.h>

/* Six output alternatives; keep as GPIO INPUT/Hi-Z until gate and trip
 * electrical design has been qualified. These are NOT output-enable ops.
 */

_Static_assert(AM13E_IO_PWM_UH_PINCM == 8U &&
               AM13E_IO_PWM_UL_PINCM == 21U &&
               AM13E_IO_PWM_VH_PINCM == 24U &&
               AM13E_IO_PWM_VL_PINCM == 2U &&
               AM13E_IO_PWM_WH_PINCM == 10U &&
               AM13E_IO_PWM_WL_PINCM == 25U &&
               IOMUX_PA8_MCPWM4_1A == 6U &&
               IOMUX_PA21_MCPWM4_1B == 5U &&
               IOMUX_PA24_MCPWM4_2A == 4U &&
               IOMUX_PA2_MCPWM4_2B == 6U &&
               IOMUX_PA10_MCPWM4_3A == 4U &&
               IOMUX_PA25_MCPWM4_3B == 12U,
               "AM13E reference IO Plan Six-step MCPWM4 pin/function drift");

/* Option 1: one high comparator, HP0/1/2 selected for logical U/V/W.
 * DACH replaces the former external neutral inputs; calibration and live
 * mux/DAC behavior remain unqualified. CMPSS1/2/3 belong to overcurrent. */
_Static_assert(AM13E_BEMF_U_PINCM==17U && AM13E_BEMF_V_PINCM==1U &&
               AM13E_BEMF_W_PINCM==29U && DL_SYSCTL_CMP_HP0==0U &&
               DL_SYSCTL_CMP_HP1==1U && DL_SYSCTL_CMP_HP2==2U,
               "Option 1 CMPSS0 phase input mapping drift");

/* PA26 is the revised development command selection; PB15 is IO-only nFAULT input.
 * PB13 Gate Enable is initialized as an INACTIVE GPIO output, with
 * no motor or audio runtime transitions. These are reference pins.
 */
_Static_assert(IOMUX_PINCM_PB13==45U &&
               IOMUX_PINCM_PA26==26U &&
               IOMUX_PINCM_PB15==47U,
               "AM13E reference IO Plan Power Stage/DShot pins changed");
