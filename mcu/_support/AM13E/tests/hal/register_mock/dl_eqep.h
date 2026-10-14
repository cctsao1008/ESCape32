#pragma once
#include AM13E_TEST_TI_EQEP_HEADER
static inline void test_eqep_clear(EQEP_Regs *p,uint16_t flags)
{ DL_EQEP_clearInterruptStatus(p,flags);p->QFLG &= ~flags; }
#define DL_EQEP_clearInterruptStatus test_eqep_clear
