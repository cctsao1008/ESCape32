#pragma once
#include <stdint.h>
typedef struct { uint32_t CTRL, LOAD, VAL; } TestSysTick;
typedef struct { uint32_t ICER[16], ICPR[16]; } TestNVIC;
typedef struct { uint32_t ICSR; } TestSCB;
extern TestSysTick test_systick;
extern TestNVIC test_nvic;
extern TestSCB test_scb;
#define SysTick (&test_systick)
#define NVIC (&test_nvic)
#define SCB (&test_scb)
#define TMU_LVF_INT_IRQn 47
#define SCB_ICSR_PENDSTCLR_Msk (UINT32_C(1) << 25)
#define SCB_ICSR_PENDSVCLR_Msk (UINT32_C(1) << 27)
void test_dsb(void);
void test_isb(void);
void test_reset(void);
#define __DSB() test_dsb()
#define __ISB() test_isb()
#define __NOP() ((void)0)
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __set_PRIMASK(uint32_t x) { (void)x; }
#define NVIC_SystemReset() test_reset()
