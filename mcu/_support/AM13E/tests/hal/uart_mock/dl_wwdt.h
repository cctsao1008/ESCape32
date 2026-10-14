#pragma once
#include <stdbool.h>
#define WWDT0 ((void*)0)
#define WWDT_WWDTCTL0_WINDOW0_SIZE_0 0
#define DL_WWDT_WINDOW_PERIOD_0 0
#define DL_WWDT_CLOCK_DIVIDE_1 1
#define DL_WWDT_TIMER_PERIOD_15_BITS 15
#define DL_WWDT_RUN_IN_SLEEP 1
void DL_WWDT_enablePower(void *w);
bool DL_WWDT_isPowerEnabled(void *w);
bool DL_WWDT_isRunning(void *w);
void DL_WWDT_initWatchdogMode(void *w,int divide,int bits,int sleep,int window0,int window1);
void DL_WWDT_restart(void *w);
