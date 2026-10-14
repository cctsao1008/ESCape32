#pragma once
#include AM13E_TEST_TI_DMA_HEADER
static inline void test_dma_clear(DMA_Regs *d,uint32_t flags)
{ DL_DMA_clearInterruptStatus(d,flags);*(volatile uint32_t *)(uintptr_t)&d->CPU_INT.MIS &= ~flags; }
#define DL_DMA_clearInterruptStatus test_dma_clear
