/*
 * E62 AM13E23019 IO Plan for TI Review v1.0 — PROVISIONAL pin mapping.
 * Source: E62_AM13E23019_IO_Plan_for_TI_Review_v1.0.xlsx, rows 9–25.
 * Pin mapping is user-selected for FW1 porting, NOT electrical sign-off.
 *
 * This file identifies real MCU pad/PINCM and analog comparator selections.
 * It must NOT imply verified PB13 gate-enable polarity, non-overlap/deadtime,
 * MOSFET driver logic, or independent overcurrent MCPWM Trip routing.
 */
#pragma once
#ifndef AM13E
#error "E62 IO Plan is only applicable to AM13E FW1"
#endif
#ifndef AM13E_E62_IO_PLAN_V1
#error "Provisional E62 IO Plan must be explicitly selected by the build"
#endif
#include <soc.h>
#include <dl_sysctl.h>

/* Six output alternatives; keep as GPIO INPUT/Hi-Z until gate and trip
 * electrical design has been qualified. These are NOT output-enable ops.
 */
#define AM13E_IO_PWM_UH_PINCM IOMUX_PINCM_PA8
#define AM13E_IO_PWM_UL_PINCM IOMUX_PINCM_PA11
#define AM13E_IO_PWM_VH_PINCM IOMUX_PINCM_PA9
#define AM13E_IO_PWM_VL_PINCM IOMUX_PINCM_PA30
#define AM13E_IO_PWM_WH_PINCM IOMUX_PINCM_PA10
#define AM13E_IO_PWM_WL_PINCM IOMUX_PINCM_PA31

_Static_assert(AM13E_IO_PWM_UH_PINCM == 8U &&
               AM13E_IO_PWM_UL_PINCM == 11U &&
               AM13E_IO_PWM_VH_PINCM == 9U &&
               AM13E_IO_PWM_VL_PINCM == 30U &&
               AM13E_IO_PWM_WH_PINCM == 10U &&
               AM13E_IO_PWM_WL_PINCM == 31U &&
               IOMUX_PA8_MCPWM0_1A == 7U &&
               IOMUX_PA11_MCPWM0_1B == 7U &&
               IOMUX_PA9_MCPWM0_2A == 7U &&
               IOMUX_PA30_MCPWM0_2B == 7U &&
               IOMUX_PA10_MCPWM0_3A == 7U &&
               IOMUX_PA31_MCPWM0_3B == 5U,
               "E62 IO Plan Six-step MCPWM0 pin/function drift");

/* Rel17 logical phase 1=U / 2=V / 3=W.
 * BEMF phase -> comparator: U=0, V=1, W=3 (distinct and fixed).
 * CMPSS high comparator positive pin: U PA17 HP0, V PA3 HP0,
 * W PA16 HP1; virtual-neutral COM negative: PA4 HN1, PA2 HN0,
 * PA18 HN0. Analog inputs do NOT use digital IOMUX function 7.
 */
#define AM13E_BEMF_PHASE1_CMPSS_IDX 0U
#define AM13E_BEMF_PHASE2_CMPSS_IDX 1U
#define AM13E_BEMF_PHASE3_CMPSS_IDX 3U

#define AM13E_BEMF_CMP0_HP_PINCM IOMUX_PINCM_PA17
#define AM13E_BEMF_CMP0_HN_PINCM IOMUX_PINCM_PA4
#define AM13E_BEMF_CMP1_HP_PINCM IOMUX_PINCM_PA3
#define AM13E_BEMF_CMP1_HN_PINCM IOMUX_PINCM_PA2
#define AM13E_BEMF_CMP3_HP_PINCM IOMUX_PINCM_PA16
#define AM13E_BEMF_CMP3_HN_PINCM IOMUX_PINCM_PA18

#define AM13E_BEMF_CMP0_HP_MUX DL_SYSCTL_CMP_HP0
#define AM13E_BEMF_CMP0_HN_MUX DL_SYSCTL_CMP_HN1
#define AM13E_BEMF_CMP1_HP_MUX DL_SYSCTL_CMP_HP0
#define AM13E_BEMF_CMP1_HN_MUX DL_SYSCTL_CMP_HN0
#define AM13E_BEMF_CMP3_HP_MUX DL_SYSCTL_CMP_HP1
#define AM13E_BEMF_CMP3_HN_MUX DL_SYSCTL_CMP_HN0

_Static_assert(AM13E_BEMF_CMP0_HP_PINCM==17U &&
               AM13E_BEMF_CMP0_HN_PINCM==4U &&
               AM13E_BEMF_CMP1_HP_PINCM==3U &&
               AM13E_BEMF_CMP1_HN_PINCM==2U &&
               AM13E_BEMF_CMP3_HP_PINCM==16U &&
               AM13E_BEMF_CMP3_HN_PINCM==18U &&
               AM13E_BEMF_CMP0_HP_MUX==DL_SYSCTL_CMP_HP0 &&
               AM13E_BEMF_CMP0_HN_MUX==DL_SYSCTL_CMP_HN1 &&
               AM13E_BEMF_CMP1_HP_MUX==DL_SYSCTL_CMP_HP0 &&
               AM13E_BEMF_CMP1_HN_MUX==DL_SYSCTL_CMP_HN0 &&
               AM13E_BEMF_CMP3_HP_MUX==DL_SYSCTL_CMP_HP1 &&
               AM13E_BEMF_CMP3_HN_MUX==DL_SYSCTL_CMP_HN0,
               "E62 IO Plan BEMF analog pin/mux drift");

/* The PB14-only FW1 interface and PB15 nFAULT remain unchanged.
 * PB13 is RESERVED (polarity/driver not yet qualified), never driven.
 */
_Static_assert(IOMUX_PINCM_PB13==45U &&
               IOMUX_PINCM_PB14==46U &&
               IOMUX_PINCM_PB15==47U,
               "E62 IO Plan Power Stage/DShot pins changed");
