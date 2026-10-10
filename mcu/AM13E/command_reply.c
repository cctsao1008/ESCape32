/* ESCape32 Rel17 BiDShot hardware: ECAP0 RX -> TIMG4/DMA0/GPIO46 TX -> RX.
 * PB14 requires a contention-safe external 3.3V/5V bidirectional front end.
 * Board timing/physical signaling is pending oscilloscope qualification.
 */
#include "command_reply.h"
#include "bidir_timing.h"
#include "io_backend.h"
#include "clock_backend.h"
#include "command_capture.h"
#include <soc.h>
#include <dl_timer.h>
#include <dl_dma.h>
#include <dl_gpio.h>
#include <dl_ecap.h>
#include <dl_sysctl.h>
#include <stddef.h>
extern void hard_fault_handler(void);
#define TX_TIMER TIMG4
#define TX_DMA DMA0
#define TX_CH 2U
#define TX_PIN DL_GPIO_PIN(14U)
#define TX_CLK (AM13E_APP_MCLK_HZ/2U)
/* AM13E230x SDK dl_dma.h advertises DATA_ERROR, but hw_dma.h
 * does NOT define DMA_IMASK_DATAERR_SET on this device.
 * Do not invent a hardware data-error IRQ bit; preserve channel
 * completion and address-error interrupt handling as implemented.
 */
#define TX_DMA_FLAGS (DL_DMA_INTERRUPT_CHANNEL2 | DL_DMA_INTERRUPT_ADDR_ERROR)
_Static_assert(DL_DMA_TRIGGER_SOURCE_TIMG4_0_GEN_EVENT1==34U,"TIMG4 DMA trigger mismatch");
_Static_assert(AM13E_BIDIR_DMA_TRANSFERS==23U,"Telemetry symbol count mismatch");

static uint32_t tx_words[AM13E_BIDIR_DMA_TRANSFERS] __attribute__((aligned(4)));
static volatile uint32_t state,tx_completed,tx_rejected,timeout_ticks;
static uint32_t symbol_ticks;
enum {TX_IDLE,TX_DELAY,TX_STREAM};

static void fail_closed(void)
{
    DL_Timer_stopCounter(TX_TIMER);
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,DL_TIMER_EVENT_ZERO_EVENT);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_GPIO_disableOutput(GPIO1,TX_PIN);
    __disable_irq();
    hard_fault_handler();
    for(;;){__NOP();}
}
static void timer_setup(uint32_t ticks,DL_TIMER_TIMER_MODE mode)
{
    DL_Timer_TimerConfig cfg={
        .timerMode=mode,.period=ticks-1U,.startTimer=DL_TIMER_STOP,
        .genIntermInt=DL_TIMER_INTERM_INT_DISABLED,.counterVal=0U
    };
    DL_Timer_initTimerMode(TX_TIMER,&cfg);
}
void am13e_pb14_bidir_tx_init(void)
{
    /* Existing clock backend provides 200MHz MCLK and 100MHz MCLK2/BUSCLK. */
    if((SYSCTL->SOCLOCK.MCLKCFG&SYSCTL_MCLKCFG_MCLKDIVCFG_MASK)!=
       (uint32_t)DL_SYSCTL_MCLK_DIV_2_DIV_4) fail_closed();
    DL_Timer_enablePower(TX_TIMER);
    if(!DL_Timer_isPowerEnabled(TX_TIMER)) fail_closed();
    DL_Timer_ClockConfig clk={
        .clockSel=DL_TIMER_CLOCK_BUSCLK,.divideRatio=DL_TIMER_CLOCK_DIVIDE_1,
        .prescale=0U
    };
    DL_Timer_setClockConfig(TX_TIMER,&clk);
    DL_Timer_enableClock(TX_TIMER);
    if(!DL_Timer_isClockEnabled(TX_TIMER)) fail_closed();
    timer_setup(TX_CLK/1000000U,DL_TIMER_TIMER_MODE_ONE_SHOT);
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,DL_TIMER_EVENT_ZERO_EVENT);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_disableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);

    DL_DMA_resetChannel(TX_DMA,TX_CH);
    DL_DMA_configTransfer(TX_DMA,TX_CH,DL_DMA_SINGLE_TRANSFER_MODE,
        DL_DMA_NORMAL_MODE,DL_DMA_WIDTH_WORD,DL_DMA_WIDTH_WORD,
        DL_DMA_ADDR_INCREMENT,DL_DMA_ADDR_UNCHANGED);
    DL_DMA_setTrigger(TX_DMA,TX_CH,(uint8_t)DL_DMA_TRIGGER_SOURCE_TIMG4_0_GEN_EVENT1,
                      DL_DMA_TRIGGER_TYPE_EXTERNAL);
    DL_DMA_setDestAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)&GPIO1->DOUTTGL31_0);
    DL_DMA_setSrcAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)tx_words);
    DL_DMA_setTransferSize(TX_DMA,TX_CH,AM13E_BIDIR_DMA_TRANSFERS);
    DL_DMA_clearInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_enableInterrupt(TX_DMA,TX_DMA_FLAGS);
    state=TX_IDLE;
    timeout_ticks=0U;
    NVIC_SetPriority(TIMG4_0_INT_IRQn,0U);
    NVIC_SetPriority(DMA0_INT_IRQn,0U);
    NVIC_ClearPendingIRQ(TIMG4_0_INT_IRQn);
    NVIC_ClearPendingIRQ(DMA0_INT_IRQn);
    NVIC_EnableIRQ(TIMG4_0_INT_IRQn);
    NVIC_EnableIRQ(DMA0_INT_IRQn);
}
int am13e_pb14_bidir_tx_busy(void){return state!=TX_IDLE;}
int am13e_pb14_bidir_tx_start(uint32_t final_edge,uint32_t rx_bit_ticks,
                              uint32_t capture_hz)
{
    if(state!=TX_IDLE){++tx_rejected;return 0;}
    const uint32_t ticks=am13e_bidir_tx_period_ticks(rx_bit_ticks,capture_hz,TX_CLK);
    if(!ticks){++tx_rejected;return 0;}
    uint8_t levels[AM13E_BIDIR_DMA_LEVELS];
    am13e_app_io_bidir_telemetry_levels(levels);
    am13e_bidir_toggle_plan(levels,TX_PIN,tx_words);
    /* Actual CEVT4 trailing edge -> TIMG4 TX deadline. Reuse the
     * host-regressed unsigned-wrap/clock-domain policy, never start
     * a late or sub-16-clock reply on PB14.
     */
    const uint32_t remaining=am13e_bidir_turnaround_ticks(
        final_edge,DL_ECAP_getTimeStampCounter(ECAP0),
        capture_hz,TX_CLK);
    if(!remaining){++tx_rejected;return 0;}
    symbol_ticks=ticks;
    timeout_ticks=0U;
    state=TX_DELAY;
    NVIC_DisableIRQ(ECAP0_INT_IRQn);
    DL_ECAP_disableTimeStampCapture(ECAP0);
    DL_Timer_stopCounter(TX_TIMER);
    timer_setup(remaining,DL_TIMER_TIMER_MODE_ONE_SHOT);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_enableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_startCounter(TX_TIMER);
    return 1;
}
void TIMG4_0_IRQHandler(void)
{
    const uint32_t ev=DL_Timer_getEnabledInterruptStatus(
        TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_disableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    if(state!=TX_DELAY || !ev) fail_closed();
    DL_Timer_stopCounter(TX_TIMER);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_DMA_setSrcAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)tx_words);
    DL_DMA_setTransferSize(TX_DMA,TX_CH,AM13E_BIDIR_DMA_TRANSFERS);
    DL_DMA_clearInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_enableChannel(TX_DMA,TX_CH);
    /* Initial Rel17 NRZI level is HIGH; DMA toggles PB14 only. */
    DL_GPIO_setPins(GPIO1,TX_PIN);
    DL_GPIO_enableOutput(GPIO1,TX_PIN);
    state=TX_STREAM;
    timer_setup(symbol_ticks,DL_TIMER_TIMER_MODE_PERIODIC);
    DL_Timer_enableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,DL_TIMER_EVENT_ZERO_EVENT);
    DL_Timer_startCounter(TX_TIMER);
}
void DMA0_IRQHandler(void)
{
    const uint32_t status=DL_DMA_getEnabledInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_clearInterruptStatus(TX_DMA,status);
    if((status&DL_DMA_INTERRUPT_ADDR_ERROR) ||
       state!=TX_STREAM ||
       !(status&DL_DMA_INTERRUPT_CHANNEL2)) fail_closed();
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,DL_TIMER_EVENT_ZERO_EVENT);
    DL_Timer_stopCounter(TX_TIMER);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_GPIO_disableOutput(GPIO1,TX_PIN);
    state=TX_IDLE;
    ++tx_completed;
    am13e_pb14_resume_rx();
}
void am13e_pb14_bidir_tx_status(uint32_t *completed, uint32_t *rejected)
{
    if (completed != NULL) *completed = tx_completed;
    if (rejected != NULL) *rejected = tx_rejected;
}
void am13e_pb14_bidir_tx_systick(void)
{
    if(state!=TX_IDLE && ++timeout_ticks>16U) fail_closed();
}
