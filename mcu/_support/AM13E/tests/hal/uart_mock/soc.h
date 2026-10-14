#pragma once
/* Host register/IRQ boundaries only; never a board profile. */
#include "../../app/aux_mock/soc.h"
#ifdef AM13E_TEST_TI_PINMAP_HEADER
#include "hw_pinmap.h"
#else
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
#define IOMUX_PINCM_PA22 22U
#define IOMUX_PA8_MCPWM4_1A 6U
#define IOMUX_PA21_MCPWM4_1B 5U
#define IOMUX_PA24_MCPWM4_2A 4U
#define IOMUX_PA2_MCPWM4_2B 6U
#define IOMUX_PA10_MCPWM4_3A 4U
#define IOMUX_PA25_MCPWM4_3B 12U
#define IOMUX_PA8_MCPWM0_1A 7U
#define IOMUX_PA11_MCPWM0_1B 7U
#define IOMUX_PA9_MCPWM0_2A 7U
#define IOMUX_PA30_MCPWM0_2B 7U
#define IOMUX_PA10_MCPWM0_3A 7U
#define IOMUX_PA31_MCPWM0_3B 5U
#define IOMUX_PINCM_PA26 26U
#define IOMUX_PA26_TIMG4_0_CCP0 3U
#define IOMUX_PA26_UC0_RX_SCL_SCLK 8U
#define IOMUX_PA26_UC0_TX_SDA_PICO 9U
#define IOMUX_PA26_UC3_TX_SDA_PICO 10U
#define IOMUX_PA26_UC3_RX_SCL_SCLK 11U
#endif
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

void NVIC_ClearPendingIRQ(int irq);

#define DOUTTGL31_0 latch
typedef struct { struct { uint32_t MCLKCFG; } SOCLOCK; } TestSysctl;
extern TestSysctl test_sysctl;
#define SYSCTL (&test_sysctl)

#define IOMUX_PINCM_PF_MASK 63U


typedef struct { uint32_t CTRL,LOAD,VAL; } TestSysTick;
extern TestSysTick test_systick;
#define SysTick (&test_systick)
