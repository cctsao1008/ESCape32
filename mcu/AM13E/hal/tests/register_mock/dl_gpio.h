#pragma once
#include AM13E_TEST_TI_GPIO_HEADER
/* Emulate GPIO atomic aliases only. Actual SDK writes still execute. */
static inline void test_gpio_set(GPIO_Regs *g,uint32_t m)
{ DL_GPIO_setPins(g,m);g->DOUT31_0|=m; }
static inline void test_gpio_clear(GPIO_Regs *g,uint32_t m)
{ DL_GPIO_clearPins(g,m);g->DOUT31_0&=~m; }
static inline void test_gpio_enable(GPIO_Regs *g,uint32_t m)
{ DL_GPIO_enableOutput(g,m);g->DOE31_0|=m; }
static inline void test_gpio_disable(GPIO_Regs *g,uint32_t m)
{ DL_GPIO_disableOutput(g,m);g->DOE31_0&=~m; }
#define DL_GPIO_setPins test_gpio_set
#define DL_GPIO_clearPins test_gpio_clear
#define DL_GPIO_enableOutput test_gpio_enable
#define DL_GPIO_disableOutput test_gpio_disable
static inline void test_gpio_irq_clear(GPIO_Regs *g,uint32_t m)
{ DL_GPIO_clearInterruptStatus(g,m);*(volatile uint32_t *)(uintptr_t)&g->CPU_INT.MIS &= ~m; }
#define DL_GPIO_clearInterruptStatus test_gpio_irq_clear
