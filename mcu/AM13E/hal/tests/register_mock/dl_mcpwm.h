#pragma once
#include AM13E_TEST_TI_MCPWM_HEADER
void test_global_load(MCPWM_Regs *p);
static inline void test_load_clear(MCPWM_Regs *p)
{ DL_MCPWM_clearGlobalLoadOneShotLatch(p);*(volatile uint32_t *)(uintptr_t)&p->GLDOSHTSTS=0U;p->GLDOSHTCTL=0U; }
static inline void test_load_arm(MCPWM_Regs *p)
{ DL_MCPWM_setGlobalLoadOneShotLatch(p);*(volatile uint32_t *)(uintptr_t)&p->GLDOSHTSTS=MCPWM_GLDOSHTSTS_OSHTLATCH_MASK;p->GLDOSHTCTL=0U; }
static inline void test_load_force(MCPWM_Regs *p)
{ DL_MCPWM_forceGlobalLoadOneShotEvent(p);test_global_load(p); }
static inline void test_irq_clear(MCPWM_Regs *p,uint16_t flags)
{ DL_MCPWM_clearInterrupt(p,flags);p->INTFLAG&=~(uint32_t)flags; }
static inline void test_global_irq_clear(MCPWM_Regs *p)
{ DL_MCPWM_clearGlobalInterrupt(p);p->INTFLAG&=~MCPWM_INTFLAG_INT_MASK; }
#define DL_MCPWM_clearGlobalLoadOneShotLatch test_load_clear
#define DL_MCPWM_setGlobalLoadOneShotLatch test_load_arm
#define DL_MCPWM_forceGlobalLoadOneShotEvent test_load_force
#define DL_MCPWM_clearInterrupt test_irq_clear
#define DL_MCPWM_clearGlobalInterrupt test_global_irq_clear

void test_shadow_written(MCPWM_Regs *p);
static inline void test_aq_shadow(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o,uint16_t a)
{ DL_MCPWM_setActionQualifierActionCompleteShadow(p,o,a);test_shadow_written(p); }
static inline void test_cmp_shadow(MCPWM_Regs *p,DL_MCPWM_COUNTER_COMPARE_MODULE o,uint16_t a)
{ DL_MCPWM_setCounterCompareShadowValue(p,o,a);test_shadow_written(p); }
static inline void test_prd_shadow(MCPWM_Regs *p,uint16_t a)
{ DL_MCPWM_setTimeBasePeriodShadow(p,a);test_shadow_written(p); }
#define DL_MCPWM_setActionQualifierActionCompleteShadow test_aq_shadow
#define DL_MCPWM_setCounterCompareShadowValue test_cmp_shadow
#define DL_MCPWM_setTimeBasePeriodShadow test_prd_shadow

void test_aq_forced(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o);
static inline void test_force_aq(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o)
{ DL_MCPWM_forceActionQualifierSWAction(p,o);test_aq_forced(p,o); }
#define DL_MCPWM_forceActionQualifierSWAction test_force_aq
