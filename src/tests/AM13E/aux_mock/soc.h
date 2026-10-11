#pragma once
#include <stdint.h>
typedef struct { uint32_t power, input, latch, output; } GPIO_Regs;
extern GPIO_Regs test_gpio[3];
#define GPIO0 (&test_gpio[0])
#define GPIO1 (&test_gpio[1])
#define GPIO2 (&test_gpio[2])
typedef struct { uint32_t ICSR, SCR; } TestSCB;
extern TestSCB test_scb;
#define SCB (&test_scb)
#define SCB_ICSR_PENDSVSET_Msk (UINT32_C(1)<<28U)
#define SCB_SCR_SLEEPONEXIT_Msk 2U
static inline uint32_t __get_PRIMASK(void) { return 1U; }
static inline void __set_PRIMASK(uint32_t x) { (void)x; }
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#define __WFI() ((void)0)
void test_halt(void);
#define __NOP() test_halt()
