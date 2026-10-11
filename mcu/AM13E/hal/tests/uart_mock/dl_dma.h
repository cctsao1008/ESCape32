#pragma once
#include <stdint.h>
#define DMA0 ((void*)0)
#define DMA0_INT_IRQn 2
#define DL_DMA_TRIGGER_SOURCE_TIMG4_0_GEN_EVENT1 34U
#define DL_DMA_INTERRUPT_CHANNEL2 4U
#define DL_DMA_INTERRUPT_ADDR_ERROR 8U
#define DL_DMA_SINGLE_TRANSFER_MODE 0U
#define DL_DMA_NORMAL_MODE 0U
#define DL_DMA_WIDTH_WORD 0U
#define DL_DMA_ADDR_INCREMENT 1U
#define DL_DMA_ADDR_UNCHANGED 0U
#define DL_DMA_TRIGGER_TYPE_EXTERNAL 0U
extern unsigned test_dma_disables;
static inline void DL_DMA_disableChannel(void *d,unsigned c){(void)d;assert(c==2U);++test_dma_disables;}
static inline void DL_DMA_resetChannel(void *d,unsigned c){(void)d;assert(c==2U);}
static inline void DL_DMA_configTransfer(void *d,unsigned c,unsigned a,unsigned b,unsigned e,unsigned f,unsigned g,unsigned h)
{(void)d;(void)c;(void)a;(void)b;(void)e;(void)f;(void)g;(void)h;}
static inline void DL_DMA_setTrigger(void *d,unsigned c,unsigned s,unsigned t){(void)d;(void)c;assert(s==34U);(void)t;}
static inline void DL_DMA_setDestAddr(void *d,unsigned c,uint32_t v){(void)d;(void)c;(void)v;}
static inline void DL_DMA_setSrcAddr(void *d,unsigned c,uint32_t v){(void)d;(void)c;(void)v;}
static inline void DL_DMA_setTransferSize(void *d,unsigned c,unsigned n){(void)d;(void)c;assert(n==23U);}
static inline void DL_DMA_clearInterruptStatus(void *d,unsigned m){(void)d;(void)m;}
static inline void DL_DMA_enableInterrupt(void *d,unsigned m){(void)d;(void)m;}
static inline void DL_DMA_enableChannel(void *d,unsigned c){(void)d;(void)c;assert(0);}
static inline unsigned DL_DMA_getEnabledInterruptStatus(void *d,unsigned m){(void)d;return m;}
