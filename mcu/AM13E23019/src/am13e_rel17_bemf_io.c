/* AM13E DriverLib BEMF I/O, deliberately without a second state machine. */
#include "am13e_rel17_bemf_io.h"
#include "dl_ecap.h"
#include "dl_timerg.h"

static am13e_rel17_capture_callback_t on_capture;
static am13e_rel17_due_callback_t on_due;

bool am13e_rel17_bemf_bind(am13e_rel17_capture_callback_t capture,
                          am13e_rel17_due_callback_t due)
{
    if (!capture || !due) return false;
    on_capture = capture;
    on_due = due;
    return true;
}

void am13e_rel17_bemf_arm_delay(uint32_t ticks)
{
    /* Tick units must already be converted to TIMG12's clock domain. */
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_setLoadValue(TIMG12, ticks);
    DL_TimerG_setTimerCount(TIMG12, ticks);
    DL_TimerG_startCounter(TIMG12);
}

void am13e_rel17_bemf_cancel_delay(void)
{
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}

void am13e_rel17_bemf_ecap0_irq(void)
{
    uint16_t sources = DL_ECAP_getInterruptSource(ECAP0);
    if (!(sources & DL_ECAP_ISR_SOURCE_CEVT1)) return;
    uint32_t elapsed = DL_ECAP_getEventTimeStamp(ECAP0, DL_ECAP_EVENT_1);
    DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    if (on_capture) on_capture(elapsed);
}

void am13e_rel17_bemf_timg12_irq(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG12) != DL_TIMERG_IIDX_ZERO)
        return;
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_stopCounter(TIMG12);
    if (on_due) on_due();
}
