/* E62 AM13E TIMG12 delay timer. IRQ vector hookup belongs to SysConfig. */
#include <stdint.h>
#include "dl_timerg.h"
#include "am13e_bemf_io.h"
#include "am13e_timg12.h"

void am13e_timg12_init(void)
{
    DL_TimerG_enablePower(TIMG12);
    DL_TimerG_reset(TIMG12);
    DL_Timer_ClockConfig clock = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0U
    };
    DL_TimerG_setClockConfig(TIMG12, &clock);
    DL_TimerG_enableClock(TIMG12);
    DL_Timer_TimerConfig mode = {
        .timerMode = DL_TIMER_TIMER_MODE_ONE_SHOT,
        .period = 0xFFFFFFFFU,
        .startTimer = DL_TIMER_STOP,
        .genIntermInt = DL_TIMER_INTERM_INT_DISABLED,
        .counterVal = 0U
    };
    DL_TimerG_initTimerMode(TIMG12, &mode);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableInterrupt(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}
void am13e_timg12_irq_callback(void)
{
    am13e_bemf_timg12_irq();
}
