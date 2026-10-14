#pragma once
#include_next <dl_cmpss_lite.h>
void test_dach_written(CMPSS_LITE_Regs *p);
static inline void test_dach_shadow(CMPSS_LITE_Regs *p,uint16_t value)
{ DL_CMPSSLITE_setDACValueHigh(p,value);test_dach_written(p); }
#define DL_CMPSSLITE_setDACValueHigh test_dach_shadow
