/* PA26 native TIMG4 CCP0 receive. TRM SPRUJF2B 18.2.3/18.4, table
 * 11-3: both-edge capture -> GEN_EVENT1 -> DMA source 34. Channel 2
 * is loaned to BiDShot TX only after capture requests are disabled.
 * SINGLE DMA never wraps into a CPU-owned buffer. SysTick drains stable
 * prefixes; completion drains the final prefix before rearming.
 * The silicon has no capture FIFO/overcapture counter: flags cannot prove
 * a CC overwrite before a DMA read. Bus throughput/IRQ latency remain
 * qualification requirements, especially at DShot1200.
 * Historical pb14 decoder/API names denote software interfaces only.
 */
#include "command_capture.h"
#include "command_decode.h"
#include "io_backend.h"
#include "serial_uart_rx.h"
#include "input_watchdog.h"
#include "command_reply.h"
#include "command_input_reference.h"
#include "clock_backend.h"
#include <dl_timer.h>
#include <dl_dma.h>
#include <dl_gpio.h>
#include <stddef.h>

extern void hard_fault_handler(void);
#define RX_TIMER IOTIM
#define RX_CH IOTIM_DMA_CHANNEL
#define RX_HZ (AM13E_APP_MCLK_HZ/2U)
#define RX_EVENTS DL_TIMER_EVENT_CC0_UP_EVENT
#define RX_DMA_FLAGS (DL_DMA_INTERRUPT_CHANNEL2|DL_DMA_FULL_CH_INTERRUPT_EARLY_CHANNEL2|DL_DMA_INTERRUPT_ADDR_ERROR)
_Static_assert(RX_HZ==100000000U && IOTIM_DMA_TRIGGER==34U,
    "PA26 TIMG4 clock/DMA routing changed");
static uint32_t edges[IOTIM_CAPTURE_SIZE] __attribute__((aligned(4)));
static AM13E_PB14_Decoder decoder;
static volatile uint32_t initialized,epoch,suspended,service_owns_input;
static unsigned consumed;
static uint32_t start_edge,last_edge,capture_pairs,capture_overruns,late_capture_groups;
static uint32_t invalid_capture_groups,max_capture_age_ticks,unexpected_gpio1_irqs;
static uint8_t inverted_rx,capture_desynced;
static uint32_t observation_stamp;
static void input_fail_closed(void)
{ __disable_irq();hard_fault_handler();for(;;){__NOP();} }

uint32_t am13e_pa26_rx_now(void)
{
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    uint32_t high=epoch;
    uint32_t low=DL_Timer_getTimerCount(RX_TIMER)&0xffffU;
    if((DL_Timer_getRawInterruptStatus(RX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT)!=0U) && low<0x8000U)
        high+=0x10000U;
    __set_PRIMASK(mask);return high+low;
}
void am13e_pa26_rx_suspend(void)
{
    DL_Timer_disableEvent(RX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearEventsStatus(RX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_DMA_disableChannel(IOTIM_DMA,RX_CH);
    DL_Timer_stopCounter(RX_TIMER);
    DL_Timer_setCaptureCompareCtl(RX_TIMER,DL_TIMER_CC_MODE_COMPARE,DL_TIMER_CC_CCOND_NOCAPTURE,IOTIM_CAPTURE_CHANNEL);
    DL_Timer_disableInterrupt(RX_TIMER,UINT32_MAX);
    DL_Timer_clearInterruptStatus(RX_TIMER,UINT32_MAX);
    DL_DMA_clearInterruptStatus(IOTIM_DMA,RX_DMA_FLAGS);
    NVIC_ClearPendingIRQ(IOTIM_IRQ);NVIC_ClearPendingIRQ(IOTIM_DMA_IRQ);
    suspended=1U;
}
static void dma_arm(void)
{
    DL_Timer_disableEvent(RX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_Timer_clearEventsStatus(RX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_DMA_disableChannel(IOTIM_DMA,RX_CH);
    DL_DMA_configTransfer(IOTIM_DMA,RX_CH,DL_DMA_SINGLE_TRANSFER_MODE,DL_DMA_NORMAL_MODE,
        DL_DMA_WIDTH_WORD,DL_DMA_WIDTH_WORD,DL_DMA_ADDR_UNCHANGED,DL_DMA_ADDR_INCREMENT);
    DL_DMA_setTrigger(IOTIM_DMA,RX_CH,IOTIM_DMA_TRIGGER,DL_DMA_TRIGGER_TYPE_EXTERNAL);
    DL_DMA_setSrcAddr(IOTIM_DMA,RX_CH,(uint32_t)(uintptr_t)&RX_TIMER->COUNTERREGS.CC_01[0]);
    DL_DMA_setDestAddr(IOTIM_DMA,RX_CH,(uint32_t)(uintptr_t)edges);
    DL_DMA_setTransferSize(IOTIM_DMA,RX_CH,IOTIM_CAPTURE_SIZE);
    DL_DMA_Full_Ch_setEarlyInterruptThreshold(IOTIM_DMA,RX_CH,DL_DMA_EARLY_INTERRUPT_THRESHOLD_HALF);
    DL_DMA_enableInterrupt(IOTIM_DMA,RX_DMA_FLAGS);
    DL_DMA_clearInterruptStatus(IOTIM_DMA,RX_DMA_FLAGS);
    consumed=0U;__DMB();DL_DMA_enableChannel(IOTIM_DMA,RX_CH);
    DL_Timer_enableEvent(RX_TIMER,DL_TIMER_GEN_EVENT1,RX_EVENTS);
}
void am13e_pb14_resume_rx(void)
{
    if(!initialized || service_owns_input)input_fail_closed();
    am13e_pa26_rx_suspend();
    DL_GPIO_disableOutput(COMMAND_GPIO,COMMAND_PIN);
    DL_GPIO_initPeripheralInputFunction(COMMAND_PINCM,COMMAND_CAPTURE_FUNCTION);
    if(!DL_GPIO_isInputEnabled(COMMAND_PINCM) ||
       DL_GPIO_getPeripheralFunctionBits(COMMAND_PINCM)!=COMMAND_CAPTURE_FUNCTION)input_fail_closed();
    DL_Timer_CaptureConfig c={.captureMode=DL_TIMER_CAPTURE_MODE_EDGE_TIME_UP,
        .period=0xffffU,.startTimer=DL_TIMER_STOP,.edgeCaptMode=DL_TIMER_CAPTURE_EDGE_DETECTION_MODE_RISING,
        .inputChan=DL_TIMER_INPUT_CHAN_0,.inputInvMode=inverted_rx?DL_TIMER_CC_INPUT_INV_INVERT:DL_TIMER_CC_INPUT_INV_NOINVERT};
    DL_Timer_initCaptureMode(RX_TIMER,&c);
    DL_Timer_disableCaptureCompareInputFilter(RX_TIMER,IOTIM_CAPTURE_CHANNEL);
    DL_Timer_setCaptureCompareCtl(RX_TIMER,DL_TIMER_CC_MODE_CAPTURE,
        DL_TIMER_CC_ZCOND_NONE|DL_TIMER_CC_ACOND_TIMCLK|DL_TIMER_CC_LCOND_NONE|DL_TIMER_CC_CCOND_TRIG_EDGE,IOTIM_CAPTURE_CHANNEL);
    DL_Timer_setCaptureCompareCtl(RX_TIMER,DL_TIMER_CC_MODE_COMPARE,DL_TIMER_CC_CCOND_NOCAPTURE,DL_TIMER_CC_1_INDEX);
    DL_Timer_setCounterValueAfterEnable(RX_TIMER,DL_TIMER_COUNT_AFTER_EN_ZERO);
    DL_Timer_setTimerCount(RX_TIMER,0U);epoch=0U;
    observation_stamp=0U;
    am13e_pb14_decoder_abort(&decoder);
    dma_arm();
    DL_Timer_clearInterruptStatus(RX_TIMER,UINT32_MAX);
    DL_Timer_enableInterrupt(RX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_enableEvent(RX_TIMER,DL_TIMER_GEN_EVENT1,RX_EVENTS);
    capture_desynced=0U;suspended=0U;DL_Timer_startCounter(RX_TIMER);
}
unsigned am13e_pa26_blackout_begin(void)
{
    if(__get_PRIMASK()!=1U)input_fail_closed();
    if(!initialized || service_owns_input)return 1U;
    if(am13e_pb14_bidir_tx_busy() || suspended)return 0U;
    am13e_pa26_rx_suspend();
    am13e_pb14_decoder_abort(&decoder);
    return 2U;
}
void am13e_pa26_blackout_end(unsigned token)
{
    if(__get_PRIMASK()!=1U || token<1U || token>2U)input_fail_closed();
    if(token==2U)am13e_pb14_resume_rx();
}
static void capture_quarantine(void)
{
    DL_Timer_disableEvent(RX_TIMER,DL_TIMER_GEN_EVENT1,UINT32_MAX);
    DL_DMA_disableChannel(IOTIM_DMA,RX_CH);
    ++late_capture_groups;capture_desynced=1U;
    am13e_pb14_decoder_abort(&decoder);
}
void initio(void)
{
    DL_GPIO_enablePower(COMMAND_GPIO);
    DL_GPIO_disableOutput(COMMAND_GPIO,COMMAND_PIN);
    DL_GPIO_initDigitalInput(COMMAND_PINCM);
    inverted_rx=DL_GPIO_readPins(COMMAND_GPIO,COMMAND_PIN)!=0U;
    am13e_pb14_decoder_reset(&decoder,RX_HZ,inverted_rx);
    am13e_app_io_watchdog_prepare();
    am13e_pb14_bidir_tx_init(); /* Establishes verified 100 MHz TIMG4 clock. */
    initialized=1U;am13e_pb14_resume_rx();
    NVIC_SetPriority(IOTIM_IRQ,0U);NVIC_SetPriority(IOTIM_DMA_IRQ,0U);
    NVIC_EnableIRQ(IOTIM_IRQ);NVIC_EnableIRQ(IOTIM_DMA_IRQ);
    const unsigned mode=am13e_app_io_input_mode();
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    if(mode==0U && !am13e_app_serial_rx_init(0U))input_fail_closed();
#endif
    if(mode>=2U && mode<=7U){
        if(!am13e_app_pb14_service_takeover() || !am13e_app_serial_rx_init(mode))input_fail_closed();
    }
#else
    if(mode>=2U && mode<=7U)input_fail_closed(); /* No invented UART route. */
#endif
}
static void drain_captured(void)
{
    if(suspended || service_owns_input || capture_desynced)return;
    const unsigned remaining=DL_DMA_getTransferSize(IOTIM_DMA,RX_CH);
    if(remaining>IOTIM_CAPTURE_SIZE)input_fail_closed();
    const unsigned available=IOTIM_CAPTURE_SIZE-remaining;
    if(available<consumed)input_fail_closed();
    __DMB();
    const uint32_t now=am13e_pa26_rx_now();
    /* DMA stores only low 16 bits, not an overflow epoch. A prefix
     * surviving an unserviced wrap cannot be dated uniquely. The ZERO
     * owner quarantines such prefixes BEFORE clearing its sticky flag.
     * Also reject a full-cycle service gap even with an extended epoch.
     * Never fabricate recent edges by reducing an ambiguous age modulo
     * 65536. Quiet-line recovery rearms the existing SINGLE buffer.
     */
    if(available>consumed &&
       (now-observation_stamp>=0x10000U ||
        DL_Timer_getRawInterruptStatus(RX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT))) {
        capture_quarantine();return;
    }
    observation_stamp=now;
    if(available==IOTIM_CAPTURE_SIZE && (DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU)!=(edges[IOTIM_CAPTURE_SIZE-1U]&0xffffU)){
        ++capture_overruns;capture_desynced=1U;am13e_pb14_decoder_abort(&decoder);return;
    }
    if(available>consumed){
        const uint32_t cc=DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU;
        if(cc!=(edges[available-1U]&0xffffU))return; /* Pending DMA beat; retry. */
        const unsigned level=DL_GPIO_readPins(COMMAND_GPIO,COMMAND_PIN)!=0U;
        if(DL_DMA_getTransferSize(IOTIM_DMA,RX_CH)!=remaining ||
           (DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU)!=cc)return;
        /* A lost/noisy edge must not complement a complete DShot packet:
         * bitwise-complemented data can also have a valid CRC. Verify
         * observed final edge polarity before forwarding this prefix. */
        const unsigned expected=(available&1U)?!inverted_rx:inverted_rx;
        if(level!=expected){
            ++invalid_capture_groups;capture_desynced=1U;
            am13e_pb14_decoder_abort(&decoder);return;
        }
    }
    for(;consumed<available;++consumed){
        /* A stable DMA prefix was sampled within one 655.36 us counter
         * cycle. The separate ZERO interrupt extends Servo frame time;
         * pulse widths never become ISR-entry timestamps. */
        const uint32_t stamp=now-(uint16_t)((uint16_t)now-(uint16_t)edges[consumed]);
        if(!(consumed&1U)){start_edge=stamp;continue;}
        const uint32_t width=stamp-start_edge;
        if(!width || width>RX_HZ/400U){++invalid_capture_groups;am13e_pb14_decoder_abort(&decoder);continue;}
        const uint32_t age=now-stamp;
        if(age>max_capture_age_ticks)max_capture_age_ticks=age;
        if(am13e_app_io_input_mode()==0U && inverted_rx &&
           am13e_pa26_decoder_reply_due(&decoder,start_edge)){
            (void)am13e_pb14_bidir_tx_start(stamp,start_edge-last_edge,RX_HZ);
        }
        last_edge=start_edge;
        const unsigned old=decoder.rx_mode;
        am13e_pb14_decoder_pulse(&decoder,start_edge,stamp,am13e_app_io_servo_pulse,am13e_app_io_dshot_packet);
        ++capture_pairs;
        if(am13e_app_io_input_mode()==0U && old==AM13E_PB14_RX_UNDECIDED && decoder.rx_mode!=old)am13e_app_io_watchdog_arm();
        if(suspended){++consumed;break;} /* Current valid frame owns the TX handover. */
    }
    if(available==IOTIM_CAPTURE_SIZE){
        if(!suspended)dma_arm();
    }
}
static void drain(void)
{
    /* DMA/SysTick/TIMG4 RX all run at priority0, verified by the startup
     * IRQ barrier. They cannot preempt one another; PendSV handover masks
     * IRQs and stops capture first. SINGLE DMA appends but cannot overwrite
     * the stable prefix before this owner rearms it. No global masking is
     * needed around decoding/Core callbacks. */
    drain_captured();
}
void am13e_pa26_rx_dma_irq(void)
{
    const uint32_t flags=DL_DMA_getEnabledInterruptStatus(IOTIM_DMA,RX_DMA_FLAGS);
    if(!flags)return;
    DL_DMA_clearInterruptStatus(IOTIM_DMA,flags);
    if(flags&DL_DMA_INTERRUPT_ADDR_ERROR)input_fail_closed();
    if(!suspended && (flags&(DL_DMA_INTERRUPT_CHANNEL2|DL_DMA_FULL_CH_INTERRUPT_EARLY_CHANNEL2)))drain();
}
void am13e_pa26_rx_timer_irq(void)
{
    const uint32_t flags=DL_Timer_getEnabledInterruptStatus(RX_TIMER,DL_TIMER_INTERRUPT_ZERO_EVENT);
    if(!flags || suspended)return;
    if(!capture_desynced &&
       IOTIM_CAPTURE_SIZE-DL_DMA_getTransferSize(IOTIM_DMA,RX_CH)>consumed)
        capture_quarantine();
    DL_Timer_clearInterruptStatus(RX_TIMER,flags);epoch+=0x10000U;
    observation_stamp=epoch+(DL_Timer_getTimerCount(RX_TIMER)&0xffffU);
}
void am13e_app_pb14_systick(void)
{
    if(!initialized)return;
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
    am13e_app_serial_rx_systick();
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    am13e_app_serial_cli_entry_poll(DL_GPIO_readPins(COMMAND_GPIO,COMMAND_PIN)!=0U,
        DL_DMA_getTransferSize(IOTIM_DMA,RX_CH)!=IOTIM_CAPTURE_SIZE,decoder.rx_mode!=AM13E_PB14_RX_UNDECIDED);
#endif
#endif
    am13e_pb14_bidir_tx_systick();
    if(service_owns_input || suspended)return;
    if(capture_desynced){
        const uint32_t now=am13e_pa26_rx_now();
        const uint32_t cc=DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU;
        if((DL_GPIO_readPins(COMMAND_GPIO,COMMAND_PIN)!=0U)==inverted_rx &&
           (uint16_t)((uint16_t)now-(uint16_t)cc)>RX_HZ/300000U*4U &&
           (DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU)==cc){
            dma_arm();capture_desynced=0U;
        }
        return;
    }
    drain_captured();
    if(!suspended && !capture_desynced){
        const uint32_t now=am13e_pa26_rx_now();
        const uint32_t period=decoder.last_bit_period?decoder.last_bit_period:
            decoder.rx_mode==AM13E_PB14_RX_DSHOT?RX_HZ/(300000U<<decoder.dshot_rate):0U;
        const uint32_t cc=DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU;
        am13e_pb14_decoder_idle(&decoder,now,am13e_app_io_dshot_packet);
        /* A truncated/noisy DShot frame must not shift the next frame's
         * edge pairs or DMA ownership. Rebase only on an observed quiet
         * idle line, stable DMA prefix and unchanged capture register. */
        if(decoder.rx_mode==AM13E_PB14_RX_DSHOT && period && consumed &&
           (uint16_t)((uint16_t)now-(uint16_t)cc)>period*4U &&
           (DL_GPIO_readPins(COMMAND_GPIO,COMMAND_PIN)!=0U)==inverted_rx &&
           DL_DMA_getTransferSize(IOTIM_DMA,RX_CH)==IOTIM_CAPTURE_SIZE-consumed &&
           (DL_Timer_getCaptureCompareValue(RX_TIMER,IOTIM_CAPTURE_CHANNEL)&0xffffU)==cc){
            am13e_pb14_decoder_abort(&decoder);dma_arm();
        }
    }
}
bool am13e_app_pb14_service_takeover(void)
{
    if(!initialized || __get_PRIMASK()!=1U || am13e_pb14_bidir_tx_busy())return false;
    if(service_owns_input)return true;
    am13e_pa26_rx_suspend();am13e_pb14_decoder_abort(&decoder);service_owns_input=1U;return true;
}
void am13e_app_io_on_gpio1_interrupt(uint32_t pending)
{ ++unexpected_gpio1_irqs;(void)pending;input_fail_closed(); }
void am13e_app_pb14_status(AM13E_PB14_Status *out)
{
    if(out==NULL)return;
    *out=(AM13E_PB14_Status){.capture_pairs=capture_pairs,.capture_overruns=capture_overruns,
        .late_capture_groups=late_capture_groups,.max_capture_age_ticks=max_capture_age_ticks,
        .invalid_capture_groups=invalid_capture_groups,.good_pwm=decoder.good_pwm,.good_dshot_rx=decoder.good_dshot,
        .rejected=decoder.rejected,.unexpected_gpio1_irqs=unexpected_gpio1_irqs,.capture_ticks_per_us=100U,
        .inverted_rx=inverted_rx,.initialized=(uint8_t)initialized};
    am13e_app_io_watchdog_status(&out->watchdog_armed,&out->watchdog_valid_feeds);
    am13e_pb14_bidir_tx_status(&out->tx_dma_completed,&out->tx_rejected);
}
