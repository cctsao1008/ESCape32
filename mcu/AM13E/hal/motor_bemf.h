#pragma once
#if !defined(AM13E)
#error "AM13E-only ECAP1/CMPSS BEMF runtime"
#endif
void am13e_app_motor_bemf_init(void);
void am13e_app_motor_bemf_abort(void);
/* Associate a pending MCPWM image with sensing only after its ZERO load. */
void am13e_app_motor_bemf_stage(unsigned code);
int am13e_app_motor_bemf_publication_pending(void);
int am13e_app_motor_bemf_carrier_zero(void);
#include <stdint.h>
int am13e_app_motor_bemf_reference_set(uint16_t code);
int am13e_app_motor_bemf_reference_ready(void);
/* Rel17 STM32G431 TIM2: 168 MHz / (PSC20+1) = 8 MHz,
 * ARR = (1U << (IFTIM_XRES + 16)) - 1 = 262143, XRES=2.
 * 262144 / 8 MHz = 32768us. Do not reduce to 16-bit ARR.
 */
#define AM13E_BEMF_REL17_TIMEOUT_US UINT32_C(32768)
/* Existing 16kHz SysTick bounds missing-edge timeout supervision. */
void am13e_app_motor_bemf_tick(void);
void ECAP1_IRQHandler(void);

#ifdef SW_BLANKING
/* Native equivalent of Rel17 CCR4=IFTIM_ICFL<<2. The existing AM13E
 * integration uses the G431 timing reference: 1024 / 168MHz. CMPC is
 * internal to the already-owned MCPWM4, not another timer or output pad. */
#define AM13E_BEMF_BLANK_TICKS UINT32_C(610) /* ceil(1024*100/168) */
void am13e_app_motor_bemf_pwm_event(void);
int am13e_app_motor_bemf_blanking_enabled(void);
#endif
