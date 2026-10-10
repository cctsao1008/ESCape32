/* TIMG12 sole owner: Rel17 sine and BEMF commutation one-shot. */
#pragma once
#if !defined(AM13E)
#error "AM13E motor timer only"
#endif
void am13e_app_motor_timing_init(void);
/* Physical TIMG12 one-shot cancellation; not comparator routing disable. */
void am13e_app_motor_timing_cancel(void);
void TIMG12_0_IRQHandler(void);
