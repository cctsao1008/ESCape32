#pragma once
#if !defined(AM13E)
#error "AM13E-only ECAP1/CMPSS BEMF runtime"
#endif
void am13e_app_motor_bemf_init(void);
void am13e_app_motor_bemf_abort(void);
#include <stdint.h>
/* Rel17 STM32G431 TIM2: 168 MHz / (PSC20+1) = 8 MHz,
 * ARR = (1U << (IFTIM_XRES + 16)) - 1 = 262143, XRES=2.
 * 262144 / 8 MHz = 32768us. Do not reduce to 16-bit ARR.
 */
#define AM13E_BEMF_REL17_TIMEOUT_US UINT32_C(32768)
/* Existing 16kHz SysTick bounds missing-edge timeout supervision. */
void am13e_app_motor_bemf_tick(void);
void ECAP1_IRQHandler(void);
