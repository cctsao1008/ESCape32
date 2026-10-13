/* ESCape32 Rel17 BiDShot hardware: TIMG4/DMA capture RX -> TIMG4/DMA0/GPIO26 TX -> RX.
 * PA26 requires a contention-safe external 3.3V/5V bidirectional front end.
 * Board timing/physical signaling is pending oscilloscope qualification.
 */
#include "command_reply.h"
#include "bidir_timing.h"
#include "io_backend.h"
#include "clock_backend.h"
#include "command_capture.h"
#include "serial_uart_rx.h"
#include <soc.h>
#include <dl_timer.h>
#include <dl_dma.h>
#include <dl_gpio.h>
#include "command_input_reference.h"
#include <dl_sysctl.h>
#include <stddef.h>
extern void hard_fault_handler(void);
#define TX_TIMER IOTIM
#define TX_DMA IOTIM_DMA
#define TX_CH IOTIM_DMA_CHANNEL
#define TX_PIN COMMAND_PIN
#define TX_CLK (AM13E_APP_MCLK_HZ/2U)
/* AM13E230x SDK dl_dma.h advertises DATA_ERROR, but hw_dma.h
 * does NOT define DMA_IMASK_DATAERR_SET on this device.
 * Do not invent a hardware data-error IRQ bit; preserve channel
 * completion and address-error interrupt handling as implemented.
 */
#define TX_DMA_FLAGS (DL_DMA_INTERRUPT_CHANNEL2 | DL_DMA_INTERRUPT_ADDR_ERROR)
_Static_assert(IOTIM_DMA_TRIGGER==34U,"TIMG4 DMA trigger mismatch");
_Static_assert(AM13E_BIDIR_DMA_TRANSFERS==23U,"Telemetry symbol count mismatch");

static uint32_t tx_words[AM13E_BIDIR_DMA_TRANSFERS] __attribute__((aligned(4)));
static volatile uint32_t state,tx_completed,tx_rejected,timeout_ticks;
static uint32_t symbol_ticks;
enum {TX_IDLE,TX_DELAY,TX_STREAM};
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
static bool serial_timer_owner,tx_initialized;
unsigned am13e_pb14_tx_timer_irq_priority(void)
{ return serial_timer_owner?1U:0U; }
/* Initialization-only transfer: no live timer/DMA owner can be displaced. */
bool am13e_pb14_release_timer_for_sbus2(void)
{
    if (__get_PRIMASK()!=1U || !tx_initialized || state!=TX_IDLE || serial_timer_owner ||
        am13e_app_io_input_mode()!=4U) return false;
    DL_Timer_stopCounter(TX_TIMER);
    DL_Timer_disableInterrupt(TX_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_disableEvent(TX_TIMER, DL_TIMER_GEN_EVENT1, DL_TIMER_EVENT_ZERO_EVENT);
    DL_DMA_disableChannel(TX_DMA, TX_CH);
    DL_DMA_clearInterruptStatus(TX_DMA, TX_DMA_FLAGS);
    DL_Timer_clearInterruptStatus(TX_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(IOTIM_IRQ);
    serial_timer_owner=true;
    return true;
}
#endif

static void fail_closed(void)
{
    DL_Timer_stopCounter(TX_TIMER);
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearEventsStatus(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_GPIO_disableOutput(COMMAND_GPIO,TX_PIN);
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
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(tx_initialized)fail_closed();
#endif
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
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearEventsStatus(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_disableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);

    DL_DMA_resetChannel(TX_DMA,TX_CH);
    DL_DMA_configTransfer(TX_DMA,TX_CH,DL_DMA_SINGLE_TRANSFER_MODE,
        DL_DMA_NORMAL_MODE,DL_DMA_WIDTH_WORD,DL_DMA_WIDTH_WORD,
        DL_DMA_ADDR_INCREMENT,DL_DMA_ADDR_UNCHANGED);
    DL_DMA_setTrigger(TX_DMA,TX_CH,(uint8_t)IOTIM_DMA_TRIGGER,
                      DL_DMA_TRIGGER_TYPE_EXTERNAL);
    DL_DMA_setDestAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)&COMMAND_GPIO->DOUTTGL31_0);
    DL_DMA_setSrcAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)tx_words);
    DL_DMA_setTransferSize(TX_DMA,TX_CH,AM13E_BIDIR_DMA_TRANSFERS);
    DL_DMA_clearInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_enableInterrupt(TX_DMA,TX_DMA_FLAGS);
    state=TX_IDLE;
    timeout_ticks=0U;
    /* Rel17 DMA TX done prepares the following reply payload. Prime
     * the first one here; every later frame is prepared at TX done,
     * not after decoding that frame's current throttle command.
     */
    uint8_t prepared[AM13E_BIDIR_DMA_LEVELS];
    am13e_app_io_bidir_telemetry_levels(prepared);
    am13e_bidir_toggle_plan(prepared,TX_PIN,tx_words);
    NVIC_SetPriority(IOTIM_IRQ,0U);
    NVIC_SetPriority(IOTIM_DMA_IRQ,0U);
    NVIC_ClearPendingIRQ(IOTIM_IRQ);
    NVIC_ClearPendingIRQ(IOTIM_DMA_IRQ);
    NVIC_EnableIRQ(IOTIM_IRQ);
    NVIC_EnableIRQ(IOTIM_DMA_IRQ);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    tx_initialized=true;
#endif
}
int am13e_pb14_bidir_tx_busy(void){return state!=TX_IDLE;}
int am13e_pb14_bidir_tx_start(uint32_t final_edge,uint32_t rx_bit_ticks,
                              uint32_t capture_hz)
{
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(serial_timer_owner){++tx_rejected;return 0;}
#endif
    if(state!=TX_IDLE){++tx_rejected;return 0;}
    const uint32_t ticks=am13e_bidir_tx_period_ticks(rx_bit_ticks,capture_hz,TX_CLK);
    if(!ticks){++tx_rejected;return 0;}
    /* tx_words was primed at init or prepared after LAST TX
     * completion. Do not change telemetry value/rep while the
     * current RX CRC or throttle command is being processed.
     */
    /* Actual final CC0 trailing edge -> TIMG4 TX deadline. Reuse the
     * host-regressed unsigned-wrap/clock-domain policy, never start
     * a late or sub-16-clock reply on PA26.
     */
    /* The RX counter owns final-edge -> snapshot elapsed time. Start the
     * CPU reconfiguration interval AFTER that snapshot, so acquisition
     * work cannot also be deducted from the remaining deadline. These
     * consecutive clock-domain observations are not a pin-time guarantee. */
    const uint32_t counter_now=am13e_pa26_rx_now();
    const uint32_t cycle_start=SysTick->VAL;
    const uint32_t remaining=am13e_bidir_turnaround_ticks(
        final_edge,counter_now,
        capture_hz,TX_CLK);
    if(!remaining){++tx_rejected;return 0;}
    symbol_ticks=ticks;
    timeout_ticks=0U;
    state=TX_DELAY;
    am13e_pa26_rx_suspend();
    DL_Timer_setCaptureCompareCtl(TX_TIMER,DL_TIMER_CC_MODE_COMPARE,DL_TIMER_CC_CCOND_NOCAPTURE,DL_TIMER_CC_0_INDEX);
    DL_Timer_setCaptureCompareCtl(TX_TIMER,DL_TIMER_CC_MODE_COMPARE,DL_TIMER_CC_CCOND_NOCAPTURE,DL_TIMER_CC_1_INDEX);
    DL_Timer_stopCounter(TX_TIMER);
    timer_setup(remaining,DL_TIMER_TIMER_MODE_ONE_SHOT);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_enableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    /* TIMG4 is stopped while its capture registers become TX. Measure
     * that real interval with the existing 200MHz SysTick counter, rather
     * than treating reconfiguration as instantaneous or guessing a delay.
     * Only one SysTick wrap can occur inside the <30us reply budget. */
    const uint32_t cycle_end=SysTick->VAL;
    const uint32_t elapsed=cycle_start>=cycle_end?cycle_start-cycle_end:
        cycle_start+(SysTick->LOAD+1U)-cycle_end;
    const uint32_t used=(elapsed+1U)/2U;
    if(used+16U>=remaining){
        ++tx_rejected;state=TX_IDLE;am13e_pb14_resume_rx();return 0;
    }
    DL_Timer_setLoadValue(TX_TIMER,remaining-used-1U);
    __DSB();DL_Timer_startCounter(TX_TIMER);
    /* Publication itself also consumes budget. If load/start MMIO or an
     * interruption crosses the deadline, cancel before GPIO ownership.
     * ISR delivery and pad propagation still require physical measurement. */
    const uint32_t published=SysTick->VAL;
    const uint32_t total=cycle_start>=published?cycle_start-published:
        cycle_start+(SysTick->LOAD+1U)-published;
    if((total+1U)/2U>=remaining){
        ++tx_rejected;state=TX_IDLE;am13e_pb14_resume_rx();return 0;
    }
    return 1;
}
void TIMG4_0_IRQHandler(void)
{
    if(state==TX_IDLE){
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
        if(!serial_timer_owner)
#endif
        {am13e_pa26_rx_timer_irq();return;}
    }
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(serial_timer_owner){am13e_app_serial_sbus2_timer_irq();return;}
#endif
    const uint32_t ev=DL_Timer_getEnabledInterruptStatus(
        TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_clearInterruptStatus(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_disableInterrupt(TX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    if(!ev)return;
    if(state!=TX_DELAY) fail_closed();
    DL_Timer_stopCounter(TX_TIMER);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_DMA_configTransfer(TX_DMA,TX_CH,DL_DMA_SINGLE_TRANSFER_MODE,DL_DMA_NORMAL_MODE,DL_DMA_WIDTH_WORD,DL_DMA_WIDTH_WORD,DL_DMA_ADDR_INCREMENT,DL_DMA_ADDR_UNCHANGED);
    DL_DMA_Full_Ch_setEarlyInterruptThreshold(TX_DMA,TX_CH,DL_DMA_EARLY_INTERRUPT_THRESHOLD_DISABLED);
    DL_DMA_disableInterrupt(TX_DMA,DL_DMA_FULL_CH_INTERRUPT_EARLY_CHANNEL2);
    DL_DMA_setSrcAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)tx_words);
    DL_DMA_setDestAddr(TX_DMA,TX_CH,(uint32_t)(uintptr_t)&COMMAND_GPIO->DOUTTGL31_0);
    DL_DMA_setTransferSize(TX_DMA,TX_CH,AM13E_BIDIR_DMA_TRANSFERS);
    DL_DMA_clearInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_enableChannel(TX_DMA,TX_CH);
    /* Initial Rel17 NRZI level is HIGH; DMA toggles PA26 only. */
    DL_GPIO_initDigitalOutput(COMMAND_PINCM);
    DL_GPIO_setPins(COMMAND_GPIO,TX_PIN);
    DL_GPIO_enableOutput(COMMAND_GPIO,TX_PIN);
    state=TX_STREAM;
    timer_setup(symbol_ticks,DL_TIMER_TIMER_MODE_PERIODIC);
    DL_Timer_enableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,DL_TIMER_EVENT_ZERO_EVENT);
    DL_Timer_startCounter(TX_TIMER);
}
void DMA0_IRQHandler(void)
{
    if(state==TX_IDLE){am13e_pa26_rx_dma_irq();return;}
    const uint32_t status=DL_DMA_getEnabledInterruptStatus(TX_DMA,TX_DMA_FLAGS);
    DL_DMA_clearInterruptStatus(TX_DMA,status);
    if(!status)return;
    if((status&DL_DMA_INTERRUPT_ADDR_ERROR) ||
       state!=TX_STREAM ||
       !(status&DL_DMA_INTERRUPT_CHANNEL2)) fail_closed();
    DL_Timer_disableEvent(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearEventsStatus(TX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_stopCounter(TX_TIMER);
    DL_DMA_disableChannel(TX_DMA,TX_CH);
    DL_GPIO_disableOutput(COMMAND_GPIO,TX_PIN);
    /* Source rel17 prepares the NEXT GCR/NRZI response after TX DMA
     * completion and before re-arming RX. No current-frame CRC
     * decision is needed to reach this state.
     */
    uint8_t prepared[AM13E_BIDIR_DMA_LEVELS];
    am13e_app_io_bidir_telemetry_levels(prepared);
    am13e_bidir_toggle_plan(prepared,TX_PIN,tx_words);
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
    if(state!=TX_IDLE && ++timeout_ticks>16U){
        /* Transport timeout is not a fabricated DMA completion. Cancel
         * both request owners, release the pad and resynchronize RX. */
        am13e_pa26_rx_suspend();
        DL_GPIO_disableOutput(COMMAND_GPIO,TX_PIN);
        state=TX_IDLE;++tx_rejected;timeout_ticks=0U;
        am13e_pb14_resume_rx();
    }
}
