#pragma once
#if !defined(AM13E)
#error "AM13E-only ECAP1/CMPSS BEMF runtime"
#endif
void am13e_app_motor_bemf_init(void);
void am13e_app_motor_bemf_abort(void);
/* Called from existing 16kHz SysTick; implements Rel17 16-bit BEMF timeout. */
void am13e_app_motor_bemf_tick(void);
void ECAP1_IRQHandler(void);
