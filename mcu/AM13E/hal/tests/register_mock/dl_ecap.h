#pragma once
#include AM13E_TEST_TI_ECAP_HEADER
static inline void test_ecap_clear(ECAP_Regs *p,uint16_t flags)
{ DL_ECAP_clearInterrupt(p,flags);p->ECFLG&=~flags; }
static inline void test_ecap_global_clear(ECAP_Regs *p)
{ DL_ECAP_clearGlobalInterrupt(p);p->ECFLG&=~ECAP_ECFLG_INT_MASK; }
#define DL_ECAP_clearInterrupt test_ecap_clear
#define DL_ECAP_clearGlobalInterrupt test_ecap_global_clear
