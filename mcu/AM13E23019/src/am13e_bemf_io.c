/* AM13E DriverLib BEMF I/O, deliberately without a second state machine. */
#include "am13e_bemf_io.h"
#include "dl_ecap.h"
#include "dl_timerg.h"

static am13e_bemf_capture_callback_t on_capture;
static am13e_bemf_due_callback_t on_due;
static uint32_t ecap_hz, timg12_hz;

bool am13e_bemf_bind(am13e_bemf_capture_callback_t capture,
                          am13e_bemf_due_callback_t due,
                     const am13e_motor_contract_t *contract)
{
    if (!capture || !due || !am13e_contract_qualified(contract)) return false;
    ecap_hz = contract->ecap_hz;
    timg12_hz = contract->timg12_hz;
    on_capture = capture;
    on_due = due;
    return true;
}

bool am13e_bemf_arm_delay(uint32_t elapsed_ecap_ticks)
{
    uint32_t ticks;
    if (!on_capture || !on_due ||
        !am13e_clock_convert_ticks(elapsed_ecap_ticks, ecap_hz,
                                   timg12_hz, &ticks)) return false;
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_setLoadValue(TIMG12, ticks);
    DL_TimerG_setTimerCount(TIMG12, ticks);
    DL_TimerG_startCounter(TIMG12);
    return true;
}

void am13e_bemf_cancel_delay(void)
{
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}

void am13e_bemf_ecap0_irq(void)
{
    uint16_t sources = DL_ECAP_getInterruptSource(ECAP0);
    if (!(sources & DL_ECAP_ISR_SOURCE_CEVT1)) return;
    uint32_t elapsed = DL_ECAP_getEventTimeStamp(ECAP0, DL_ECAP_EVENT_1);
    DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    if (on_capture) on_capture(elapsed);
}

void am13e_bemf_timg12_irq(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG12) != DL_TIMERG_IIDX_ZERO)
        return;
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_stopCounter(TIMG12);
    if (on_due) on_due();
}
