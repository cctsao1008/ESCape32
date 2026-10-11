#pragma once
/* Host register/IRQ boundaries only; never a board profile. */
#include "../../../../../src/tests/AM13E/aux_mock/soc.h"
extern uint32_t test_primask;
#define __get_PRIMASK() test_primask
#define __set_PRIMASK(x) (test_primask=(x))
#undef __disable_irq
#undef __enable_irq
#define __disable_irq() (test_primask=1U)
#define __enable_irq() (test_primask=0U)
#define __DSB() ((void)0)
typedef struct { uint32_t RXDATA; } TestUART;
typedef struct { TestUART *uart; } TestUC;
extern TestUC test_uc;
#define UC0_INST_PTR (&test_uc)
#define UC0_INT_IRQn 0
uint32_t NVIC_GetEnableIRQ(int irq);
void NVIC_DisableIRQ(int irq);
void NVIC_EnableIRQ(int irq);
void NVIC_SetPriority(int irq,unsigned priority);
unsigned NVIC_GetPriority(int irq);
uint32_t NVIC_GetVector(int irq);
#define IOMUX_PINCM_PA2 2U
#define IOMUX_PINCM_PA3 3U
#define IOMUX_PINCM_PA4 4U
#define IOMUX_PINCM_PA6 6U
#define IOMUX_PINCM_PA8 8U
#define IOMUX_PINCM_PA9 9U
#define IOMUX_PINCM_PA10 10U
#define IOMUX_PINCM_PA11 11U
#define IOMUX_PINCM_PA16 16U
#define IOMUX_PINCM_PA17 17U
#define IOMUX_PINCM_PA18 18U
#define IOMUX_PINCM_PA28 28U
#define IOMUX_PINCM_PA30 30U
#define IOMUX_PINCM_PA31 31U
#define IOMUX_PINCM_PB13 45U
#define IOMUX_PINCM_PB14 46U
#define IOMUX_PINCM_PB15 47U
#define IOMUX_PA8_MCPWM0_1A 7U
#define IOMUX_PA11_MCPWM0_1B 7U
#define IOMUX_PA9_MCPWM0_2A 7U
#define IOMUX_PA30_MCPWM0_2B 7U
#define IOMUX_PA10_MCPWM0_3A 7U
#define IOMUX_PA31_MCPWM0_3B 5U
