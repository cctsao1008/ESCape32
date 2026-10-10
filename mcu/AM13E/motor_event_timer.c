/* AM13E Rel17 motor commutation timer: TIMG12, independent of TIMG4
 * (BiDShot TX), ECAP0 (PB14 RX), and MCPWM0 (power-stage).
 * The 100MHz BUSCLK is the accepted XTAL/PLL/MCLK divider contract.
 * A final board must measure peripheral tick rate/IRQ jitter.
 */
#include "motor_event_timer.h"
#include "motor_timer_math.h"
#include "motor_backend.h"
#include "motor_audio_hw.h"
#include "clock_backend.h"
#include <limits.h>
#include <soc.h>
#include <dl_timer.h>
#include <dl_sysctl.h>
#include <stdint.h>

#define MOTOR_TIMER TIMG12
#define MOTOR_TIMER_HZ (AM13E_APP_MCLK_HZ / 2U)
_Static_assert(MOTOR_TIMER_HZ == UINT32_C(100000000),
               "TIMG12 peripheral clock contract changed");

extern void hard_fault_handler(void);
static volatile uint32_t initialized;
static volatile uint32_t scheduled;
static volatile uint32_t serviced;
static volatile uint32_t cancelled;
static volatile uint32_t timer_armed;
static volatile uint32_t audio_pcm_timer;
static uint32_t pcm_deadline, pcm_floor_ticks, pcm_remainder, pcm_accum, pcm_rate;

static void timing_fault(void)
{
    __disable_irq();
    DL_Timer_disableInterrupt(MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_stopCounter(MOTOR_TIMER);
    hard_fault_handler();
    for (;;) { __NOP(); }
}

void am13e_app_motor_timing_init(void)
{
    if (__get_PRIMASK() == 0U ||
        (SYSCTL->SOCLOCK.MCLKCFG & SYSCTL_MCLKCFG_MCLKDIVCFG_MASK) !=
            (uint32_t)DL_SYSCTL_MCLK_DIV_2_DIV_4) timing_fault();
    DL_Timer_enablePower(MOTOR_TIMER);
    if (!DL_Timer_isPowerEnabled(MOTOR_TIMER)) timing_fault();
    DL_Timer_ClockConfig cfg = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0U
    };
    DL_Timer_setClockConfig(MOTOR_TIMER, &cfg);
    DL_Timer_enableClock(MOTOR_TIMER);
    if (!DL_Timer_isClockEnabled(MOTOR_TIMER)) timing_fault();
    DL_Timer_stopCounter(MOTOR_TIMER);
    DL_Timer_clearInterruptStatus(MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_enableInterrupt(MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    timer_armed = 0U;
    initialized = 1U;
    NVIC_SetPriority(TIMG12_0_INT_IRQn, 0U);
    NVIC_ClearPendingIRQ(TIMG12_0_INT_IRQn);
    NVIC_EnableIRQ(TIMG12_0_INT_IRQn);
    /* Global PRIMASK stays set until the qualified runtime barrier. */
}

static void schedule_us(int delay_us)
{
    if (!initialized || delay_us <= 0 || audio_pcm_timer ||
        am13e_app_motor_audio_mode()) timing_fault();
    const uint32_t ticks = am13e_motor_us_to_timer_ticks(
        (uint32_t)delay_us, MOTOR_TIMER_HZ);
    if (ticks < 2U) timing_fault();
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    /* A replacement commutation schedule must not let an obsolete
     * zero-event fire while its period and counter are being replaced.
     */
    timer_armed=0U;
    DL_Timer_stopCounter(MOTOR_TIMER);
    DL_Timer_TimerConfig cfg = {
        .timerMode = DL_TIMER_TIMER_MODE_ONE_SHOT,
        .period = ticks - 1U,
        .startTimer = DL_TIMER_STOP,
        .genIntermInt = DL_TIMER_INTERM_INT_DISABLED,
        .counterVal = 0U
    };
    DL_Timer_initTimerMode(MOTOR_TIMER, &cfg);
    DL_Timer_clearInterruptStatus(MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(TIMG12_0_INT_IRQn);
    timer_armed=1U;
    DL_Timer_startCounter(MOTOR_TIMER);
    ++scheduled;
    __set_PRIMASK(irqmask);
}

/* Cancel the actual TIMG12 one-shot. Used during Rel17 Stop/Reset and
 * fatal shutdown, before a stale motor IRQ may call nextstep().
 * This is the shared sine/commutation TIMER cancel, not a claim that
 * unknown CMPSS comparator routing or interrupt masks are disabled.
 */
void am13e_app_motor_timing_cancel(void)
{
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    timer_armed=0U;
    if (initialized) {
        DL_Timer_stopCounter(MOTOR_TIMER);
        DL_Timer_clearInterruptStatus(MOTOR_TIMER,
                                       DL_TIMER_INTERRUPT_ZERO_EVENT);
        NVIC_ClearPendingIRQ(TIMG12_0_INT_IRQn);
    }
    ++cancelled;
    __set_PRIMASK(irqmask);
}

void am13e_app_motor_sine_schedule_us(int period_us)
{
    schedule_us(period_us);
}

void am13e_app_motor_bemf_commutation_delay_us(int delay_us)
{
    schedule_us(delay_us);
}

/* Rel17 main.c calls this twice: for a computed sine->six-step phase
 * offset, and with 0xffff us on initial six-step start. Both are real
 * TIMG12 one-shot schedules, not a fake acknowledgement or motor poller.
 * TIMG4 stays exclusively owned by PB14 BiDShot TX.
 */
void am13e_app_motor_bemf_sine_exit_us(int delay_us)
{
    schedule_us(delay_us);
}

void TIMG12_0_IRQHandler(void)
{
    const uint32_t status = DL_Timer_getEnabledInterruptStatus(
        MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_clearInterruptStatus(MOTOR_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    if (!initialized) timing_fault();
    /* A cancelled one-shot can leave a latched/pending event. It must
     * never spuriously advance the original Rel17 step sequence.
     */
    if (audio_pcm_timer || !timer_armed) return;
    if ((status & DL_TIMER_INTERRUPT_ZERO_EVENT) == 0U)
        timing_fault();
    timer_armed=0U;
    DL_Timer_stopCounter(MOTOR_TIMER);
    ++serviced;
    /* Original ESCape32 Rel17 nextstep(), not an alternate algorithm. */
    am13e_app_motor_on_commutation_event();
}

/* Rel17 TIM6 PCM sample pacing via exclusive TIMG12 owner.
 * TIMG12 IRQ/commutation scheduling are disabled throughout this
 * interval; PB14 BiDShot owns separate TIMG4. Uses the real 100MHz
 * peripheral clock and Bresenham fractional-rate compensation.
 * This MUST be called from the blocking main audio path, not an IRQ.
 */
void am13e_app_motor_audio_pcm_clock_begin(uint32_t rate)
{
    if (!initialized || timer_armed || audio_pcm_timer ||
        am13e_app_motor_audio_mode()!=2U ||
        rate<1000U || rate>48000U || __get_PRIMASK()!=0U)
        timing_fault();
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    DL_Timer_stopCounter(MOTOR_TIMER);
    DL_Timer_disableInterrupt(MOTOR_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_clearInterruptStatus(MOTOR_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(TIMG12_0_INT_IRQn);
    DL_Timer_TimerConfig cfg={
       .timerMode=DL_TIMER_TIMER_MODE_PERIODIC_UP,
       .period=UINT32_MAX,
       .startTimer=DL_TIMER_STOP,
       .genIntermInt=DL_TIMER_INTERM_INT_DISABLED,
       .counterVal=0U
    };
    DL_Timer_initTimerMode(MOTOR_TIMER,&cfg);
    pcm_rate=rate;
    pcm_floor_ticks=MOTOR_TIMER_HZ/rate;
    pcm_remainder=MOTOR_TIMER_HZ%rate;
    pcm_accum=0U;
    audio_pcm_timer=1U;
    DL_Timer_startCounter(MOTOR_TIMER);
    pcm_deadline=DL_Timer_getTimerCount(MOTOR_TIMER);
    __set_PRIMASK(irqmask);
}

void am13e_app_motor_audio_pcm_sample_wait(void)
{
    if (!audio_pcm_timer || !pcm_rate ||
        am13e_app_motor_audio_mode()!=2U || __get_PRIMASK()!=0U)
        timing_fault();
    uint32_t step=pcm_floor_ticks;
    pcm_accum+=pcm_remainder;
    if(pcm_accum>=pcm_rate){
        pcm_accum-=pcm_rate;
        ++step;
    }
    pcm_deadline+=step;
    /* Wrap-safe comparison: next interval <=100000 ticks << 2^31.
     * Preserve all AU PCM samples; no fake timing or silent drop.
     * SysTick/ECAP0/PB14 higher-priority IRQs remain enabled.
     */
    while ((int32_t)(DL_Timer_getTimerCount(MOTOR_TIMER)-pcm_deadline)<0)
        __NOP();
}

void am13e_app_motor_audio_pcm_clock_end(void)
{
    if (!audio_pcm_timer || am13e_app_motor_audio_mode()!=2U)
        timing_fault();
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    DL_Timer_stopCounter(MOTOR_TIMER);
    DL_Timer_clearInterruptStatus(MOTOR_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(TIMG12_0_INT_IRQn);
    audio_pcm_timer=0U;
    pcm_rate=0U;
    timer_armed=0U;
    DL_Timer_enableInterrupt(MOTOR_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    __set_PRIMASK(mask);
}
