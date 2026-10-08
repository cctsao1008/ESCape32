/*
 * E62 FW1 BEMF event adapter.
 * Execute the real ESCape32 timing policy and TIMG12 delay register path.
 * This does not configure CMPSS input mux, eCAP capture mode or NVIC.
 * Caller MUST supply interval ticks (not an absolute eCAP timestamp) in
 * the same clock domain as TIMG12; that product clock contract is pending.
 */
#include <stdbool.h>
#include <stdint.h>
#include "dl_timerg.h"
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

bool fw1_bemf_event_setup(void (*commutate)(void),
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

bool fw1_bemf_event_capture_interval(uint32_t elapsed_ticks)
{
    return initialized &&
        am13e_bemf_event_capture(&engine, elapsed_ticks)
            == AM13E_BEMF_ACCEPTED;
}

void fw1_bemf_event_timg12_irq(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG12) != DL_TIMERG_IIDX_ZERO)
        return;
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_stopCounter(TIMG12);
    if (initialized) am13e_bemf_event_delay_elapsed(&engine);
}

void fw1_bemf_event_timeout(unsigned timer_xres)
{
    if (initialized) am13e_bemf_event_timeout(&engine, timer_xres);
}
