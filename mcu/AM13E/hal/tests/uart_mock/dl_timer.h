#pragma once
#include <soc.h>
typedef unsigned TestTimer;
extern TestTimer test_timer;
#define TIMG4 (&test_timer)
#define TIMG4_0_INT_IRQn 1
#define DL_TIMER_INTERRUPT_CC1_UP_EVENT 1U
#define DL_TIMER_CC_1_INDEX 1U
#define DL_TIMER_CLOCK_BUSCLK 1U
#define DL_TIMER_CLOCK_DIVIDE_1 0U
#define DL_TIMER_TIMER_MODE_PERIODIC_UP 1U
#define DL_TIMER_STOP 0U
#define DL_TIMER_INTERM_INT_DISABLED 0U
#define DL_TIMER_CC_MODE_COMPARE 0U
#define DL_TIMER_CC_ZCOND_NONE 0U
#define DL_TIMER_CC_LCOND_NONE 0U
#define DL_TIMER_CC_ACOND_TIMCLK 1U
#define DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE 0U
typedef struct { unsigned clockSel,divideRatio,prescale; } DL_Timer_ClockConfig;
typedef struct { unsigned timerMode,period,startTimer,genIntermInt,counterVal; } DL_Timer_TimerConfig;
unsigned DL_Timer_getTimerCount(TestTimer *t);
void DL_Timer_disableInterrupt(TestTimer *t,unsigned mask);
void DL_Timer_enableInterrupt(TestTimer *t,unsigned mask);
void DL_Timer_clearInterruptStatus(TestTimer *t,unsigned mask);
void DL_Timer_setCaptureCompareValue(TestTimer *t,unsigned value,unsigned cc);
void DL_Timer_setClockConfig(TestTimer *t,const DL_Timer_ClockConfig *c);
void DL_Timer_initTimerMode(TestTimer *t,const DL_Timer_TimerConfig *c);
void DL_Timer_setCaptureCompareCtl(TestTimer *t,unsigned mode,unsigned conditions,unsigned cc);
void DL_Timer_setCaptCompUpdateMethod(TestTimer *t,unsigned method,unsigned cc);
void DL_Timer_startCounter(TestTimer *t);
unsigned DL_Timer_getEnabledInterruptStatus(TestTimer *t,unsigned mask);

typedef unsigned DL_TIMER_TIMER_MODE;
#define DL_TIMER_TIMER_MODE_ONE_SHOT 2U
#define DL_TIMER_TIMER_MODE_PERIODIC 3U
#define DL_TIMER_INTERRUPT_ZERO_EVENT 2U
#define DL_TIMER_EVENT_ZERO_EVENT 2U
#define DL_TIMER_GEN_EVENT1 1U
extern unsigned test_timer_stops;
static inline void DL_Timer_stopCounter(TestTimer *t){(void)t;++test_timer_stops;}
static inline void DL_Timer_disableEvent(TestTimer *t,unsigned p,unsigned m){(void)t;(void)p;(void)m;}
static inline void DL_Timer_enableEvent(TestTimer *t,unsigned p,unsigned m){(void)t;(void)p;(void)m;}
static inline void DL_Timer_enablePower(TestTimer *t){(void)t;}
static inline bool DL_Timer_isPowerEnabled(TestTimer *t){(void)t;return true;}
static inline void DL_Timer_enableClock(TestTimer *t){(void)t;}
static inline bool DL_Timer_isClockEnabled(TestTimer *t){(void)t;return true;}
