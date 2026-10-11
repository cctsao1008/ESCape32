#pragma once
/* Host-only core primitives. Device registers and DriverLib are the SDK's.
 * Tests map anonymous RAM at device addresses; never /dev/mem or hardware. */
#include <stdint.h>
#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline
extern uint32_t test_primask;
void test_halt(void);
static inline uint32_t __get_PRIMASK(void){return test_primask;}
static inline void __set_PRIMASK(uint32_t m){test_primask=m;}
#define __disable_irq() (test_primask=1U)
#define __enable_irq() (test_primask=0U)
#define __NOP() test_halt()
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#define __DMB() ((void)0)
#define __WFI() ((void)0)
typedef struct { uint32_t ICSR,SCR,VTOR; } TestSCB;
extern TestSCB test_scb;
#define SCB (&test_scb)
#define SCB_ICSR_PENDSVSET_Msk (UINT32_C(1)<<28U)
#define SCB_SCR_SLEEPONEXIT_Msk 2U
void NVIC_SetPriority(IRQn_Type irq,uint32_t p);
void NVIC_ClearPendingIRQ(IRQn_Type irq);
void NVIC_EnableIRQ(IRQn_Type irq);
void NVIC_DisableIRQ(IRQn_Type irq);
uint32_t NVIC_GetEnableIRQ(IRQn_Type irq);
uint32_t NVIC_GetPriority(IRQn_Type irq);
uint32_t NVIC_GetVector(IRQn_Type irq);

#define SCB_SCR_SLEEPDEEP_Msk 4U
#define SCB_SCR_SEVONPEND_Msk 16U
