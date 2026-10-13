#pragma once
#include AM13E_TEST_TI_ECAP_HEADER
static inline void test_ecap_clear(ECAP_Regs *p,uint16_t flags)
{ DL_ECAP_clearInterrupt(p,flags);p->ECFLG&=~flags; }
static inline void test_ecap_global_clear(ECAP_Regs *p)
{ DL_ECAP_clearGlobalInterrupt(p);p->ECFLG&=~ECAP_ECFLG_INT_MASK; }
#define DL_ECAP_clearInterrupt test_ecap_clear
#define DL_ECAP_clearGlobalInterrupt test_ecap_global_clear
void test_ecap_counter_started(ECAP_Regs *p,int was_running);
static inline void test_ecap_start(ECAP_Regs *p)
{
    const int running=(p->ECCTL2&ECAP_ECCTL2_TSCTRSTOP_MASK)!=0U;
    DL_ECAP_startCounter(p);test_ecap_counter_started(p,running);
}
#define DL_ECAP_startCounter test_ecap_start
