#pragma once
#include <stdint.h>
void DL_SYSTICK_init(uint32_t ticks);
void DL_SYSTICK_enable(void);
uint32_t DL_SYSTICK_getValue(void);
