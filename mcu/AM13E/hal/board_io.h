/* Selected development routing. Scalar PINCM definitions are shared by
 * Application and Boot; no Application runtime or SDK dependency here.
 * PA26 intentionally differs from the immutable Rev1.6 PB14 originals.
 * Allocation is not electrical qualification. */
#pragma once
#include <stdint.h>
#ifndef AM13E_COMMAND_PINCM
#define AM13E_COMMAND_PINCM 26U
#endif
#define AM13E_COMMAND_GPIO GPIO0
#define AM13E_COMMAND_MASK (UINT32_C(1)<<AM13E_COMMAND_PINCM)
#define AM13E_MOTOR_GPIO GPIO0
#define AM13E_GATE_GPIO GPIO1
#define AM13E_NFAULT_GPIO GPIO0
#define AM13E_GATE_PINCM 45U
#define AM13E_NFAULT_PINCM 22U
#define AM13E_NTC_PINCM 6U
#define AM13E_VBUS_PINCM 28U
#define AM13E_XTAL1_PINCM 80U
#define AM13E_XTAL2_PINCM 81U
#define AM13E_IO_PWM_UH_PINCM 8U
#define AM13E_IO_PWM_UL_PINCM 21U
#define AM13E_IO_PWM_VH_PINCM 24U
#define AM13E_IO_PWM_VL_PINCM 2U
#define AM13E_IO_PWM_WH_PINCM 10U
#define AM13E_IO_PWM_WL_PINCM 25U
#define AM13E_BEMF_U_PINCM 17U
#define AM13E_BEMF_V_PINCM 1U
#define AM13E_BEMF_W_PINCM 29U
#define AM13E_PIN_MOTOR(p) ((p)==AM13E_IO_PWM_UH_PINCM || (p)==AM13E_IO_PWM_UL_PINCM || \
    (p)==AM13E_IO_PWM_VH_PINCM || (p)==AM13E_IO_PWM_VL_PINCM || \
    (p)==AM13E_IO_PWM_WH_PINCM || (p)==AM13E_IO_PWM_WL_PINCM)
#define AM13E_PIN_BEMF(p) ((p)==AM13E_BEMF_U_PINCM || \
    (p)==AM13E_BEMF_V_PINCM || (p)==AM13E_BEMF_W_PINCM)
/* TI Option 1 shared-board allocations: no Six-Step reuse of FOC,
 * debug, CAN, DAC, IPD or timing/fault test pads. Spare PA12/28/31
 * remain available; reservation alone does not enable a peripheral. */
#define AM13E_PIN_OPTION1_RESERVED(p) ((p)==0U || (p)==1U || (p)==3U || \
    (p)==4U || (p)==5U || (p)==6U || (p)==7U || (p)==9U || (p)==11U || \
    (p)==13U || (p)==14U || (p)==15U || (p)==16U || (p)==17U || \
    (p)==18U || (p)==19U || (p)==20U || (p)==22U || (p)==23U || \
    (p)==29U || (p)==30U || (p)==46U || (p)==47U)
#define AM13E_PIN_FIXED(p) (AM13E_PIN_OPTION1_RESERVED(p) || AM13E_PIN_MOTOR(p) || AM13E_PIN_BEMF(p) || \
    (p)==AM13E_COMMAND_PINCM || (p)==AM13E_GATE_PINCM || (p)==AM13E_NFAULT_PINCM || \
    (p)==AM13E_NTC_PINCM || (p)==AM13E_VBUS_PINCM || \
    (p)==AM13E_XTAL1_PINCM || (p)==AM13E_XTAL2_PINCM)
_Static_assert(AM13E_COMMAND_PINCM==26U,"Selected command route requires PA26, not a portable arbitrary pad");

#define AM13E_GATE_MASK (UINT32_C(1)<<(AM13E_GATE_PINCM&31U))
#define AM13E_NFAULT_MASK (UINT32_C(1)<<(AM13E_NFAULT_PINCM&31U))

#define AM13E_NFAULT_INPUTXBAR DL_XBAR_INPUT2
#define AM13E_NFAULT_PWMXBAR DL_XBAR_TRIP2
#define AM13E_NFAULT_TZ_SIGNAL DL_MCPWM_TZ_SIGNAL_OST2
#define AM13E_NFAULT_SOURCE DL_XBAR_PWM_INPUTXBAR2
