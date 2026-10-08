/*
 * E62 AM13E BEMF event adapter.
 * Execute the real ESCape32 timing policy and TIMG12 delay register path.
 * This does not configure CMPSS input mux, eCAP capture mode or NVIC.
 * Caller MUST supply interval ticks (not an absolute eCAP timestamp) in
 * the same clock domain as TIMG12; that product clock contract is pending.
 */
#include <stdbool.h>
#include <stdint.h>
#include "dl_timerg.h"
#include "dl_ecap.h"
#include "am13e_bemf_events.h"
#include "hw_bemf_am13e_events.h"

static am13e_bemf_event_engine_t engine;
static void (*on_due)(void);
static bool initialized;

static void arm_delay(void *ctx, uint32_t ticks)
{
    (void)ctx;
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_setLoadValue(TIMG12, ticks);
    DL_TimerG_setTimerCount(TIMG12, ticks);
    DL_TimerG_startCounter(TIMG12);
}
static void cancel_delay(void *ctx)
{
    (void)ctx;
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}
static void delay_complete(void *ctx)
{
    (void)ctx;
    if (on_due) on_due();
}

bool am13e_bemf_event_setup(void (*commutate)(void),
                          uint32_t interval_ticks,
                          uint32_t electrical_time,
                          unsigned timing)
{
    if (!commutate || !interval_ticks) return false;
    const am13e_bemf_event_ops_t ops = {
        arm_delay, cancel_delay, delay_complete, 0
    };
    am13e_bemf_state_t initial = {
        .interval = interval_ticks,
        .electrical_time = electrical_time,
        .sync = 0,
        .fast = false
    };
    initialized = false;
    on_due = commutate;
    initialized = am13e_bemf_event_init(&engine, &ops, initial, timing);
    return initialized;
}

bool am13e_bemf_event_capture_interval(uint32_t elapsed_ticks)
{
    return initialized &&
        am13e_bemf_event_capture(&engine, elapsed_ticks)
            == AM13E_BEMF_ACCEPTED;
}

void am13e_bemf_event_timg12_irq(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG12) != DL_TIMERG_IIDX_ZERO)
        return;
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_stopCounter(TIMG12);
    if (initialized) am13e_bemf_event_delay_elapsed(&engine);
}

void am13e_bemf_adapter_timeout(unsigned timer_xres)
{
    if (initialized) am13e_bemf_event_timeout(&engine, timer_xres);
}

/*
 * Hardware eCAP Event-1 entry for AM13E: CAP1 is a timer count, not
 * intrinsically an ESCape32 commutation interval. The caller must reset
 * the eCAP elapsed-time epoch at each commutation before arming capture.
 * The eCAP event and timer clock domains must be configured consistently.
 */
bool am13e_bemf_event_ecap0_event1(void)
{
    uint16_t status = DL_ECAP_getInterruptSource(ECAP0);
    if ((status & DL_ECAP_ISR_SOURCE_CEVT1) == 0U)
        return false;
    uint32_t captured = DL_ECAP_getEventTimeStamp(ECAP0, DL_ECAP_EVENT_1);
    DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    return am13e_bemf_event_capture_interval(captured);
}

/*
 * Start a new commutation interval epoch. CAP1 thereafter measures ticks
 * since this boundary only when eCAP was configured in absolute capture mode,
 * CAP1 reset-on-event is disabled, and its clock is calibrated to TIMG12.
 * Never treat an arbitrary free-running timestamp as elapsed time.
 */
void am13e_bemf_ecap_start_epoch(void)
{
    DL_ECAP_disableTimeStampCapture(ECAP0);
    DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    DL_ECAP_resetCounters(ECAP0);
    DL_ECAP_enableTimeStampCapture(ECAP0);
}

void am13e_bemf_ecap_configure_capture(DL_ECAP_INPUT source,
                                     DL_ECAP_EVENT_POLARITY edge)
{
    DL_ECAP_disableTimeStampCapture(ECAP0);
    DL_ECAP_selectECAPInput(ECAP0, source);
    DL_ECAP_enableCaptureMode(ECAP0);
    DL_ECAP_setCaptureMode(ECAP0, DL_ECAP_CONTINUOUS_CAPTURE_MODE,
                           DL_ECAP_EVENT_1);
    DL_ECAP_disableCounterResetOnEvent(ECAP0, DL_ECAP_EVENT_1);
    DL_ECAP_setEventPolarity(ECAP0, DL_ECAP_EVENT_1, edge);
    DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    DL_ECAP_enableInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
    am13e_bemf_ecap_start_epoch();
}
