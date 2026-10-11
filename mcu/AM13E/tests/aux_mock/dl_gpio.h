#pragma once
#include <soc.h>
#include <stdbool.h>
#define DL_GPIO_PIN(n) (UINT32_C(1)<<(n))
enum { DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE };
extern unsigned test_function[96], test_input[96], test_enable_calls;
extern unsigned test_read_toggle;
static inline void DL_GPIO_enablePower(GPIO_Regs *g) { g->power=1U; }
static inline bool DL_GPIO_isPowerEnabled(GPIO_Regs *g) { return g->power!=0U; }
static inline void DL_GPIO_disableOutput(GPIO_Regs *g,uint32_t mask) { g->output&=~mask; }
#ifdef AM13E_TEST_GPIO_ENABLE_OBSERVER
void test_output_enable(GPIO_Regs *g,uint32_t mask);
#endif
static inline void DL_GPIO_enableOutput(GPIO_Regs *g,uint32_t mask)
{
#ifdef AM13E_TEST_GPIO_ENABLE_OBSERVER
    test_output_enable(g,mask);
#endif
    ++test_enable_calls; g->output|=mask;
}
static inline void DL_GPIO_initDigitalInputFeatures(unsigned p,unsigned inv,unsigned r,unsigned w)
{ (void)inv;(void)r;(void)w;test_function[p]=1U;test_input[p]=1U; }
static inline unsigned DL_GPIO_isInputEnabled(unsigned p) { return test_input[p]; }
static inline unsigned DL_GPIO_getPeripheralFunctionBits(unsigned p) { return test_function[p]; }
static inline void DL_GPIO_initDigitalOutput(unsigned p) { test_function[p]=1U; }
static inline void DL_GPIO_clearPins(GPIO_Regs *g,uint32_t mask) { g->latch&=~mask; }
static inline void DL_GPIO_setPins(GPIO_Regs *g,uint32_t mask) { g->latch|=mask; }
static inline uint32_t DL_GPIO_readPins(GPIO_Regs *g,uint32_t mask)
{ if(test_read_toggle)g->input^=mask;return g->input&mask; }
