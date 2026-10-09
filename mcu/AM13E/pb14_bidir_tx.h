#pragma once
#if !defined(AM13E)
#error "AM13E PB14 only"
#endif
#include <stdint.h>
void am13e_pb14_bidir_tx_init(void);
int am13e_pb14_bidir_tx_start(uint32_t final_edge,uint32_t period_ticks,uint32_t capture_hz);
int am13e_pb14_bidir_tx_busy(void);
void am13e_pb14_bidir_tx_systick(void);
/* Software-only diagnostics. Physical TX verification remains mandatory. */
void am13e_pb14_bidir_tx_status(uint32_t *completed, uint32_t *rejected);
void TIMG4_0_IRQHandler(void);
void DMA0_IRQHandler(void);
