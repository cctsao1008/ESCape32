#pragma once
#include <stdint.h>
typedef struct { uint32_t ICSR, SCR; } TestSCB;
extern TestSCB test_scb;
extern uint32_t test_primask;
#define SCB (&test_scb)
#define SCB_ICSR_PENDSVSET_Msk (UINT32_C(1)<<28U)
#define SCB_SCR_SLEEPONEXIT_Msk 2U
void test_irq_disable(void);
void test_irq_restore(uint32_t mask);
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
#define __disable_irq() test_irq_disable()
#define __enable_irq() test_irq_restore(0U)
#define __set_PRIMASK(mask) test_irq_restore(mask)
#define __WFI() ((void)0)
