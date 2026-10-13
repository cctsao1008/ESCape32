#pragma once
#if !defined(AM13E)
#error "AM13E-only ECAP1/CMPSS BEMF runtime"
#endif
void am13e_app_motor_bemf_init(void);
void am13e_app_motor_bemf_abort(void);
/* Fault shutdown cancels calibration permanently until reset. */
void am13e_app_motor_bemf_fault_stop(void);
/* Associate a pending MCPWM image with sensing only after its ZERO load. */
void am13e_app_motor_bemf_stage(unsigned code);
int am13e_app_motor_bemf_publication_pending(void);
int am13e_app_motor_bemf_carrier_zero(void);
#include <stdint.h>
int am13e_app_motor_bemf_reference_set(uint16_t code);
int am13e_app_motor_bemf_reference_ready(void);
int am13e_app_motor_bemf_reference_start_ready(void);
int am13e_app_motor_bemf_reference_gate_ready(void);
typedef struct {
    unsigned configured,calculated,published,capture_synchronized;
    uint16_t expected_code,active_code;
    uint32_t sample;
} AM13E_BemfReferenceStatus;
void am13e_app_motor_bemf_reference_status(AM13E_BemfReferenceStatus *out);
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
