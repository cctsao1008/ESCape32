/* Native TI AM13E UNICOMM IRQ/FIFO RX for Rel17 input_mode 3/4/5.
 * Reference has no assigned UART, so this implementation is selected
 * ONLY by explicit AM13E_BOARD_SERIAL_RX_CONFIGURED board definitions.
 * Optional board-selected 2-wire CRSF command TX uses UNICOMM FIFO,
 * TX interrupt and EOT, without DMA. Conditional SBUS2 timing uses
 * TIMG4 only after an initialization-time transfer from BiDShot.
 * PA26 single-wire RX/TX is selected explicitly by TX_TWO_WIRE=0;
 * the same pad changes native functions only after transport ownership.
 */
#include "serial_uart_rx.h"
#include "service_reset.h"
#include "command_capture.h"
#include "motor_backend.h"
#if defined(AM13E_BOARD_SERIAL_TX_CONFIGURED) && \
    defined(AM13E_BOARD_SERIAL_TX_TWO_WIRE) && AM13E_BOARD_SERIAL_TX_TWO_WIRE==0
#define PA26_SINGLE_WIRE 1
#endif
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#include "serial_frame_rx.h"
#include "../../../src/defs.h"
#include "../../../src/exbus_autobaud.h"
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
#include "serial_tx_queue.h"
#include "crsf_deferred_command.h"
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
#include "cli_line_rx.h"
#include "../../../src/cli_entry.h"
#endif
#endif
#include "io_backend.h"
#if defined(AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED) && \
    (!defined(AM13E_BOARD_SERIAL_TX_CONFIGURED) || \
     AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED!=1)
#error "CLI requires board-selected TX and explicit entry qualification"
#endif
/* The original EXBUS single-wire transceiver remains unqualified.
 * A two-wire board-selected response is permitted only with an
 * explicit board-assigned stable identity and separate physical TX.
 */
#if defined(AM13E_BOARD_EXBUS_REPLY_ENABLED)
#if AM13E_BOARD_EXBUS_REPLY_ENABLED != 1 || \
    !defined(AM13E_BOARD_SERIAL_TX_CONFIGURED) || \
    !defined(AM13E_BOARD_EXBUS_STABLE_DEVICE_ID)
#error "EXBUS telemetry needs explicit board TX topology and stable UID"
#endif
_Static_assert(AM13E_BOARD_EXBUS_STABLE_DEVICE_ID!=0U &&
               AM13E_BOARD_EXBUS_STABLE_DEVICE_ID!=UINT32_MAX,
               "EXBUS reply cannot use placeholder device identity");
#endif
#include "board_reference_io.h"
#include "board_configuration.h"
#include "../config.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_unicommuart.h>
#include <stdint.h>
/* Binary Serial uses the original compile-time baud contract. Keep the
 * selected baud in the SDK's nonzero, 16x divisor range. */
_Static_assert(SERIAL_BR>0 && SERIAL_BR<=AM13E_BOARD_SERIAL_RX_BUSCLK_HZ/16U &&
               (UINT64_C(4)*AM13E_BOARD_SERIAL_RX_BUSCLK_HZ+SERIAL_BR/2U)/
                 (SERIAL_BR>0?SERIAL_BR:1U)<=UINT64_C(0x3fffff),
               "SERIAL_BR requires a representable nonzero native 16x UART divisor");
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
#include <dl_timer.h>
#include "command_reply.h"
#include "clock_backend.h"
#if AM13E_BOARD_SBUS2_TIMED_REPLY!=1 || \
    !defined(AM13E_BOARD_SERIAL_TX_CONFIGURED) || \
    !defined(AM13E_BOARD_SBUS2_TX_PIN_INVERT)
#error "Timed SBUS2 needs explicit TX topology and board-selected TX polarity"
#endif
_Static_assert((AM13E_BOARD_SERIAL_TX_TWO_WIRE==1 || AM13E_BOARD_SERIAL_TX_TWO_WIRE==0) &&
               AM13E_BOARD_SBUS2_TX_PIN_INVERT<=1U &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ==AM13E_APP_MCLK_HZ/2U,
               "SBUS2 requires reviewed polarity and 100MHz UART/TIMG clock");
#endif

#if AM13E_BOARD_SERIAL_RX_CONFIGURED != 1 || \
    !defined(AM13E_BOARD_SERIAL_RX_UC_INDEX) || \
    !defined(AM13E_BOARD_SERIAL_RX_PINCM) || \
    !defined(AM13E_BOARD_SERIAL_RX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_SERIAL_RX_BUSCLK_HZ) || \
    !defined(AM13E_BOARD_SERIAL_SBUS_PIN_INVERT)
#error "Board UART RX requires dedicated UC, RX pin/function, verified BUSCLK"
#endif
/* PF is a six-bit SDK field; functions9/11/13 exist on this silicon.
 * This validates encoding only. The board must still prove the UC/pad route. */
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM>0U &&
               AM13E_BOARD_SERIAL_RX_PINCM<107U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION>0U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION<=IOMUX_PINCM_PF_MASK &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ>=UINT32_C(2000000) &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ<=UINT32_C(200000000) &&
               AM13E_BOARD_SERIAL_SBUS_PIN_INVERT<=1U,
               "Native UART RX board route/function/clock invalid");
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PA26 ||
    ((AM13E_BOARD_SERIAL_RX_UC_INDEX==0 && AM13E_BOARD_SERIAL_RX_PIN_FUNCTION==8) ||
     (AM13E_BOARD_SERIAL_RX_UC_INDEX==3 && AM13E_BOARD_SERIAL_RX_PIN_FUNCTION==11)),
    "PA26 UART RX must select its actual UC0/UC3 function");
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PB14 &&
               (!AM13E_PIN_FIXED(AM13E_BOARD_SERIAL_RX_PINCM) || AM13E_BOARD_SERIAL_RX_PINCM==IO_PINCM) &&
               !AM13E_PIN_TELEM(AM13E_BOARD_SERIAL_RX_PINCM) &&
               !AM13E_PIN_HALL(AM13E_BOARD_SERIAL_RX_PINCM) &&
               !AM13E_PIN_ANALOG(AM13E_BOARD_SERIAL_RX_PINCM) &&
               !AM13E_PIN_AUX(AM13E_BOARD_SERIAL_RX_PINCM) &&
               (AM13E_BOARD_SERIAL_TX_PINCM==0U ||
                (AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_SERIAL_TX_PINCM ||
#ifdef PA26_SINGLE_WIRE
                true
#else
                false
#endif
                )) &&
               (AM13E_BOARD_OC_GPIO_PINCM==0U ||
                AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_OC_GPIO_PINCM) &&
               (AM13E_BOARD_CURRENT_SENSE_PINCM==0U ||
                AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_CURRENT_SENSE_PINCM),
               "Native serial RX pin conflicts with board-reserved IO");

#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
#if AM13E_BOARD_SERIAL_TX_CONFIGURED != 1 || \
    !defined(AM13E_BOARD_SERIAL_TX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_SERIAL_TX_TWO_WIRE)
#error "UART TX requires explicit TX pin function and board topology"
#endif
#ifdef PA26_SINGLE_WIRE
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM==26U && AM13E_BOARD_SERIAL_TX_PINCM==26U &&
    ((AM13E_BOARD_SERIAL_RX_UC_INDEX==0 && AM13E_BOARD_SERIAL_RX_PIN_FUNCTION==8 && AM13E_BOARD_SERIAL_TX_PIN_FUNCTION==9) ||
     (AM13E_BOARD_SERIAL_RX_UC_INDEX==3 && AM13E_BOARD_SERIAL_RX_PIN_FUNCTION==11 && AM13E_BOARD_SERIAL_TX_PIN_FUNCTION==10)),
    "Single-wire command UART requires actual PA26 UC0/UC3 RX/TX functions");
#else
_Static_assert(AM13E_BOARD_SERIAL_TX_TWO_WIRE==1U &&
               AM13E_BOARD_SERIAL_TX_PINCM>0U &&
               AM13E_BOARD_SERIAL_TX_PINCM<64U &&
               AM13E_BOARD_SERIAL_TX_PIN_FUNCTION>0U &&
               AM13E_BOARD_SERIAL_TX_PIN_FUNCTION<=IOMUX_PINCM_PF_MASK &&
               AM13E_BOARD_SERIAL_TX_PINCM!=AM13E_BOARD_SERIAL_RX_PINCM,
               "CRSF TX must own a distinct board-assigned two-wire pin");
#endif
#endif

#define RX_UC IO_UART
#define RX_IRQ IO_UART_IRQ
#define RX_HANDLER IO_UART_HANDLER

/* SDK 26.01's DL_UART_ERROR_OVERRUN references an undefined
 * RXDATA_OVRERR_SET; real overrun is a separate IIDX IRQ, not a
 * per-byte RXDATA flag. Inspect RXDATA only for defined byte errors.
 */
#define RX_ERRORS (UNICOMMUART_RXDATA_FRMERR_SET | \
                   UNICOMMUART_RXDATA_PARERR_SET | \
                   UNICOMMUART_RXDATA_BRKERR_SET | \
                   UNICOMMUART_RXDATA_NERR_SET)
#define RX_IRQ_ERRORS (DL_UART_INTERRUPT_OVERRUN_ERROR | \
                       DL_UART_INTERRUPT_FRAMING_ERROR | \
                       DL_UART_INTERRUPT_PARITY_ERROR | \
                       DL_UART_INTERRUPT_NOISE_ERROR)
static AM13E_SerialFrameRx rx_state;
static unsigned initialized;
static void serial_fault(void)
{ am13e_app_motor_fault_shutdown(); am13e_app_motor_fault_reset(); }
static bool receiver_selected;
/* 16 kHz MCU SysTick timebase. IRQ/PendSV share only an atomic
 * 32-bit snapshot; no serial state is reset from high-priority
 * SysTick while UART IRQ might be in flight.
 */
static volatile uint32_t serial_rx_tick;
/* Only UART IRQ creates a new EXBUS retry; PendSV owns the actual
 * peripheral configuration. SysTick supplies time and pends PendSV,
 * but never mutates UART hardware or shared RX parser.
 */
static AM13E_ExbusAutobaud exbus_baud;
static volatile uint32_t exbus_change_requested_tick;
static volatile uint32_t exbus_change_pending;

static void exbus_request_retry(void)
{
    if(rx_state.mode!=6U || exbus_change_pending)return;
    uint32_t new_baud=0U;
    if(!am13e_exbus_autobaud_bad(&exbus_baud,&new_baud))
        return;
    (void)new_baud;
    am13e_serial_frame_abort(&rx_state);
    exbus_change_requested_tick=serial_rx_tick;
    exbus_change_pending=1U;
    /* No wait for UART Busy and no synchronous clock changes in ISR.
     * The low-priority worker is re-pended after an idle guard time.
     */
}
/* Fail closed on live RX: only PendSV may touch baud registers,
 * and never while a UART character is being received or FIFO nonempty.
 */
void am13e_app_serial_exbus_service_pending(void)
{
    if(!initialized||rx_state.mode!=6U||!exbus_change_pending||
       (uint32_t)(serial_rx_tick-exbus_change_requested_tick)<4U)
        return;
    NVIC_DisableIRQ(RX_IRQ);
    __DSB();
    if(exbus_change_pending && rx_state.mode==6U &&
       DL_UART_isRXFIFOEmpty(RX_UC) && !DL_UART_isBusy(RX_UC)){
        DL_UART_disable(RX_UC);
        /* TI SDK DL_UART_configBaudRate modifies CTL0 oversampling
         * and baud divisors: UART must be disabled first.
         * No blocking DL_UART_changeConfig() is called in PendSV.
         */
        DL_UART_configBaudRate(RX_UC,
            AM13E_BOARD_SERIAL_RX_BUSCLK_HZ,exbus_baud.baud);
        DL_UART_enable(RX_UC);
        am13e_serial_frame_abort(&rx_state);
        rx_state.has_previous_byte_tick=false;
        exbus_change_pending=0U;
    }
    NVIC_EnableIRQ(RX_IRQ);
}
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
static AM13E_SerialTxQueue crsf_tx;
static AM13E_CrsfDeferredCommand crsf_cmd;
static bool crsf_tx_available;
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
static AM13E_CliLineRx cli_rx;
static bool cli_active;
static AM13E_CliEntry cli_entry;
static char cli_reply[AM13E_CLI_LINE_BYTES];
static unsigned cli_reply_length,cli_reply_offset;
#endif

/* Original CRSF command execution is deferred to PendSV.
 * Queue writes begin from PendSV, while TX/EOT drain in UART IRQ.
 */
/* PendSV calls this worker; both worker and UART IRQ use
 * the same bounded TX queue owned by this UNICOMM instance.
 */
static bool crsf_tx_start(const uint8_t *p,unsigned n);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
/* TIMG4: 16-bit free-running 1MHz counter, CC1 absolute deadlines.
 * No accumulated ISR delay and no timer/DMA borrowing while live.
 * Original Rel17: first slot 3200us, six starts 660us apart, RX at7160us.
 * This two-wire adapter mutes RX in software until the real final EOT.
 * Single-wire driver enable/Hi-Z turnaround remains a board blocker.
 */
#define SBUS2_TIMER IOTIM
#define SBUS2_IRQ DL_TIMER_INTERRUPT_CC1_UP_EVENT
static bool sbus2_timer_owned;
static volatile bool sbus2_active,sbus2_release_due;
static unsigned sbus2_slot,sbus2_count;
static uint16_t sbus2_origin,sbus2_deadline;
static uint32_t sbus2_origin_tick;
static uint16_t sbus2_now(void)
{ return (uint16_t)DL_Timer_getTimerCount(SBUS2_TIMER); }
static void sbus2_release_rx(void)
{
    if(sbus2_release_due && crsf_tx.count==0U && !DL_UART_isBusy(RX_UC)){
        sbus2_active=false;
        sbus2_release_due=false;
        am13e_serial_frame_abort(&rx_state);
        rx_state.has_previous_byte_tick=false;
    }
}
static bool sbus2_compare(uint16_t deadline)
{
    sbus2_deadline=deadline;
    DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
    DL_Timer_clearInterruptStatus(SBUS2_TIMER,SBUS2_IRQ);
    NVIC_ClearPendingIRQ(IOTIM_IRQ);
    DL_Timer_setCaptureCompareValue(SBUS2_TIMER,deadline,DL_TIMER_CC_1_INDEX);
    /* Reject a deadline passed during publication instead of waiting a wrap. */
    if((int16_t)(deadline-sbus2_now())<=0)return false;
    DL_Timer_enableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
    if((int16_t)(deadline-sbus2_now())<=0){
        DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
        return false;
    }
    return true;
}
static bool sbus2_timer_init(void)
{
    if(!am13e_pb14_release_timer_for_sbus2())return false;
    DL_Timer_ClockConfig clock={.clockSel=DL_TIMER_CLOCK_BUSCLK,
        .divideRatio=DL_TIMER_CLOCK_DIVIDE_1,.prescale=99U};
    DL_Timer_setClockConfig(SBUS2_TIMER,&clock);
    DL_Timer_TimerConfig timer={.timerMode=DL_TIMER_TIMER_MODE_PERIODIC_UP,
        .period=UINT16_MAX,.startTimer=DL_TIMER_STOP,
        .genIntermInt=DL_TIMER_INTERM_INT_DISABLED,.counterVal=0U};
    DL_Timer_initTimerMode(SBUS2_TIMER,&timer);
    DL_Timer_setCaptureCompareCtl(SBUS2_TIMER,DL_TIMER_CC_MODE_COMPARE,
        DL_TIMER_CC_ZCOND_NONE|DL_TIMER_CC_LCOND_NONE|DL_TIMER_CC_ACOND_TIMCLK,
        DL_TIMER_CC_1_INDEX);
    DL_Timer_setCaptCompUpdateMethod(SBUS2_TIMER,DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
        DL_TIMER_CC_1_INDEX);
    DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
    DL_Timer_clearInterruptStatus(SBUS2_TIMER,SBUS2_IRQ);
    NVIC_ClearPendingIRQ(IOTIM_IRQ);
    sbus2_active=sbus2_release_due=false;
    sbus2_timer_owned=true;
    /* Same priority as UART: timer TX and UART FIFO/EOT cannot interrupt
     * each other's queue mutation. Motor/ECAP/SysTick stay at priority0.
     * In BiDShot mode the original TIMG4 priority0 remains unchanged.
     */
    NVIC_SetPriority(IOTIM_IRQ,1U);
    DL_Timer_startCounter(SBUS2_TIMER);
    return true;
}
static void sbus2_begin(uint16_t received)
{
    if(!sbus2_timer_owned || sbus2_active || crsf_tx.count!=0U)return;
    sbus2_slot=0U;
    sbus2_count=6U; /* Core decides eligibility at the first slot deadline. */
    sbus2_origin=received;
    sbus2_origin_tick=serial_rx_tick;
    sbus2_active=true;
    sbus2_release_due=false;
    if(!sbus2_compare((uint16_t)(received+3200U))){
        sbus2_count=0U;
        if(sbus2_compare((uint16_t)(received+7160U)))return;
        sbus2_release_due=true;
        sbus2_release_rx();
    }
}
void am13e_app_serial_sbus2_timer_irq(void)
{
    const uint32_t event=DL_Timer_getEnabledInterruptStatus(SBUS2_TIMER,SBUS2_IRQ);
    DL_Timer_clearInterruptStatus(SBUS2_TIMER,SBUS2_IRQ);
    if(!sbus2_timer_owned || !sbus2_active || !event)return;
    /* Independent higher-priority SysTick rejects a starved UART/timer
     * delivery even after the 16-bit counter has wrapped a whole turn.
     * 128 ticks is8ms, beyond the original7160us reply window.
     */
    if((uint32_t)(serial_rx_tick-sbus2_origin_tick)>=128U){
        DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
        sbus2_release_due=true;
        sbus2_release_rx();
        return;
    }
    /* A delayed ISR must not bunch slots. 3 bytes at 100k 8E2 need360us;
     * >=300us lateness cannot fit the original 660us slot interval.
     * This is an overlap guard, NOT proof of the SBUS2 jitter limit.
     */
    const int16_t late=(int16_t)(sbus2_now()-sbus2_deadline);
    if(sbus2_slot<sbus2_count && late>=0 && late<300 &&
       crsf_tx.count==0U && !DL_UART_isBusy(RX_UC)){
        uint8_t packet[3];unsigned delay_us;
        /* Original sbusirq2 samples telemetry at EACH slot, not frame RX.
         * UART and TIMG4 have equal priority and RX is muted, so neither
         * parser nor CLI can concurrently mutate the original Core iobuf.
         */
        if(am13e_app_io_sbus2_next_slot(packet,&delay_us) &&
           delay_us==(sbus2_slot?660U:3200U) &&
           (int16_t)(sbus2_now()-sbus2_deadline)<300){
            ++sbus2_slot;
            crsf_tx_start(packet,3U);
        } else sbus2_slot=sbus2_count;
    } else sbus2_slot=sbus2_count;
    if(sbus2_slot<sbus2_count){
        if(sbus2_compare((uint16_t)(sbus2_origin+3200U+660U*sbus2_slot)))return;
        sbus2_slot=sbus2_count;
    }
    if((int16_t)((uint16_t)(sbus2_origin+7160U)-sbus2_now())>0 &&
       sbus2_compare((uint16_t)(sbus2_origin+7160U)))return;
    DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
    sbus2_release_due=true;
    sbus2_release_rx();
}
#endif

#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
static void cli_start_next_chunk(void);
#endif

void am13e_app_serial_crsf_service_pending(void)
{
    if(!crsf_tx_available||!crsf_cmd.busy||
       crsf_cmd.length==0U||crsf_tx.count!=0U)return;
    uint8_t packet[AM13E_CRSF_PENDING_MAX];
    uint8_t reply[AM13E_SERIAL_TX_MAX];
    unsigned packet_len=0U;
    if(!am13e_crsf_deferred_take(&crsf_cmd,packet,sizeof packet,
                                 &packet_len))return;
    /* This is PendSV, not the UC RX interrupt: original
     * execcrsfcmd() may execute savecfg()/resetcfg() and Flash.
     * High priority RX remains interruptible while this runs.
     */
    const unsigned n=am13e_app_io_crsf_reply(
        packet,packet_len,reply,sizeof reply);
    if(n)crsf_tx_start(reply,n);
    am13e_crsf_deferred_finish(&crsf_cmd);
}

#ifdef PA26_SINGLE_WIRE
static bool wire_tx_active;
static volatile bool wire_abort_pending;
static uint32_t wire_started;
extern void hard_fault_handler(void);
static void wire_rx(void)
{
    DL_GPIO_disableOutput(IO_GPIO,IO_PIN);
    DL_UART_setDirection(RX_UC,DL_UART_DIRECTION_RX);
    DL_GPIO_initPeripheralInputFunctionFeatures(AM13E_BOARD_SERIAL_RX_PINCM,
        AM13E_BOARD_SERIAL_RX_PIN_FUNCTION,
        rx_state.mode==4U
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
        && !cli_active
#endif
        && AM13E_BOARD_SERIAL_SBUS_PIN_INVERT ? DL_GPIO_INVERSION_ENABLE:DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE,DL_GPIO_WAKEUP_DISABLE);
    DL_UART_clearInterruptStatus(RX_UC,DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS|DL_UART_INTERRUPT_TX|DL_UART_INTERRUPT_EOT_DONE);
    DL_UART_enableInterrupt(RX_UC,DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
    wire_tx_active=false;
}
static bool wire_tx(void)
{
    if(wire_tx_active || !DL_UART_isRXFIFOEmpty(RX_UC) || DL_UART_isBusy(RX_UC))return false;
    DL_UART_disableInterrupt(RX_UC,DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
    DL_UART_setDirection(RX_UC,DL_UART_DIRECTION_TX);
    DL_GPIO_disableOutput(IO_GPIO,IO_PIN);
    DL_GPIO_initPeripheralOutputFunction(AM13E_BOARD_SERIAL_TX_PINCM,AM13E_BOARD_SERIAL_TX_PIN_FUNCTION);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    DL_GPIO_setDataInversion(AM13E_BOARD_SERIAL_TX_PINCM,
        rx_state.mode==4U
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
        && !cli_active
#endif
        && AM13E_BOARD_SBUS2_TX_PIN_INVERT ? DL_GPIO_INVERSION_ENABLE:DL_GPIO_INVERSION_DISABLE);
#endif
    __DSB();wire_tx_active=true;wire_started=serial_rx_tick;
    return true;
}
static void wire_abort(void)
{
    /* A 64-byte reply at the slowest 38400 8N1 rate needs <17 ms.
     * At 32 ms, abort the transport, never synthesize an EOT event. */
    DL_UART_disableInterrupt(RX_UC,DL_UART_INTERRUPT_TX|DL_UART_INTERRUPT_EOT_DONE);
    DL_UART_setDirection(RX_UC,DL_UART_DIRECTION_RX);
    DL_GPIO_initDigitalInput(AM13E_BOARD_SERIAL_RX_PINCM);
    DL_UART_enableFlushTxFifo(RX_UC);
    DL_UART_enableFlushRxFifo(RX_UC);
    unsigned budget=128U;
    while(budget && (!DL_UART_isTxFifoClearDone(RX_UC)||!DL_UART_isRxFifoClearDone(RX_UC)))--budget;
    if(!budget){hard_fault_handler();return;}
    DL_UART_disableFlushTxFifo(RX_UC);DL_UART_disableFlushRxFifo(RX_UC);
    crsf_tx.count=crsf_tx.sent=0U;crsf_tx.awaiting_eot=false;++crsf_tx.dropped;
    am13e_serial_frame_abort(&rx_state);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    cli_reply_length=cli_reply_offset=0U;am13e_cli_line_abort(&cli_rx);
#endif
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(sbus2_timer_owned){
        DL_Timer_disableInterrupt(SBUS2_TIMER,SBUS2_IRQ);
        DL_Timer_clearInterruptStatus(SBUS2_TIMER,SBUS2_IRQ);
        NVIC_ClearPendingIRQ(IOTIM_IRQ);
        sbus2_active=sbus2_release_due=false;sbus2_count=0U;
    }
#endif
    wire_rx();
}
#endif
void am13e_app_serial_transport_service_pending(void)
{
#ifdef PA26_SINGLE_WIRE
    if(!wire_abort_pending)return;
    /* PendSV is below UART and SBUS2: neither owner is already running.
     * SysTick only requests recovery; it must not mutate a preempted ISR. */
    const uint32_t uart_enabled=NVIC_GetEnableIRQ(RX_IRQ);
    NVIC_DisableIRQ(RX_IRQ);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    const uint32_t timer_enabled=NVIC_GetEnableIRQ(IOTIM_IRQ);
    if(sbus2_timer_owned)NVIC_DisableIRQ(IOTIM_IRQ);
#endif
    __DSB();wire_abort_pending=false;
    if(wire_tx_active && (uint32_t)(serial_rx_tick-wire_started)>=512U)wire_abort();
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(sbus2_timer_owned && timer_enabled)NVIC_EnableIRQ(IOTIM_IRQ);
#endif
    if(uart_enabled)NVIC_EnableIRQ(RX_IRQ);
#endif
}
static void crsf_tx_fill_fifo(void)
{
    for(unsigned budget=0U;budget<AM13E_SERIAL_TX_MAX;++budget){
        if(DL_UART_isTXFIFOFull(RX_UC))break;
        uint8_t b;
        if(!am13e_serial_tx_queue_next(&crsf_tx,&b))break;
        DL_UART_transmitData(RX_UC,b);
    }
    if(am13e_serial_tx_queue_waiting_eot(&crsf_tx))
        DL_UART_disableInterrupt(RX_UC,DL_UART_INTERRUPT_TX);
}
static bool crsf_tx_start(const uint8_t *p,unsigned n)
{
    /* PendSV can be preempted by this UART. Publish queue/FIFO/EOT
     * state as one transaction; keep motor and SysTick IRQs live. */
    const uint32_t enabled=NVIC_GetEnableIRQ(RX_IRQ);
    NVIC_DisableIRQ(RX_IRQ);
    __DSB();
#ifdef PA26_SINGLE_WIRE
    if(!wire_tx()){if(enabled)NVIC_EnableIRQ(RX_IRQ);return false;}
#endif
    if(!crsf_tx_available ||
       !am13e_serial_tx_queue_begin(&crsf_tx,p,n)) {
#ifdef PA26_SINGLE_WIRE
        wire_rx();
#endif
        if(enabled)NVIC_EnableIRQ(RX_IRQ);
        return false;
    }
    DL_UART_clearInterruptStatus(RX_UC,
                                 DL_UART_INTERRUPT_TX |
                                 DL_UART_INTERRUPT_EOT_DONE);
    DL_UART_enableInterrupt(RX_UC,DL_UART_INTERRUPT_EOT_DONE);
    crsf_tx_fill_fifo();
    if(!am13e_serial_tx_queue_waiting_eot(&crsf_tx))
        DL_UART_enableInterrupt(RX_UC,DL_UART_INTERRUPT_TX);
    if(enabled)NVIC_EnableIRQ(RX_IRQ);
    return true;
}
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
/* Physical entry requires the original HIGH timeout signal on PA26.
 * Never selected or auto-inferred by the unconfigured Reference profile.
 * Selected mode2 binary serial and CLI share the UART, not a parser.
 */
bool am13e_app_serial_cli_enter(void)
{
    if(!initialized && cli_entry.requested && __get_PRIMASK()==1U){
        const AM13E_CliEntry entry=cli_entry;
        if(!am13e_app_pb14_service_takeover() || !am13e_app_serial_rx_init(2U))return false;
        cli_entry=entry;
    }
    if(!initialized||cli_active||cli_entry.selected||!cli_entry.requested||
       __get_PRIMASK()!=1U||!crsf_tx_available||
       crsf_tx.count!=0U||crsf_cmd.busy||
       !am13e_app_io_service_idle() ||
       !DL_UART_isRXFIFOEmpty(RX_UC)||DL_UART_isBusy(RX_UC))
        return false;
    if(!am13e_app_pb14_service_takeover())return false;
    DL_UART_disable(RX_UC);
    /* SBUS receiver parity/stop bits must not leak into text CLI. */
    DL_UART_Config config={
        .mode=DL_UART_MODE_NORMAL,.direction=
#ifdef PA26_SINGLE_WIRE
        DL_UART_DIRECTION_RX,
#else
        DL_UART_DIRECTION_TX_RX,
#endif
        .flowControl=DL_UART_FLOW_CONTROL_NONE,.parity=DL_UART_PARITY_NONE,
        .wordLength=DL_UART_WORD_LENGTH_8_BITS,.stopBits=DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(RX_UC,&config);
    DL_UART_configBaudRate(RX_UC,AM13E_BOARD_SERIAL_RX_BUSCLK_HZ,38400U);
    /* Original text CLI is noninverted 8N1, even when entry precedes
     * SBUS receiver selection. Reset the IOMUX polarity as well as UART. */
    DL_GPIO_setDataInversion(AM13E_BOARD_SERIAL_RX_PINCM,DL_GPIO_INVERSION_DISABLE);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    DL_GPIO_setDataInversion(AM13E_BOARD_SERIAL_TX_PINCM,DL_GPIO_INVERSION_DISABLE);
#endif
    DL_UART_enable(RX_UC);
    am13e_serial_frame_abort(&rx_state);
    am13e_cli_line_init(&cli_rx);
    cli_reply_length=cli_reply_offset=0U;
    cli_active=true;
    am13e_cli_entry_select_receiver(&cli_entry);
    return true;
}
static void cli_start_next_chunk(void)
{
    if(cli_reply_offset>=cli_reply_length)return;
    const unsigned remaining=cli_reply_length-cli_reply_offset;
    const unsigned count=remaining>AM13E_SERIAL_TX_MAX?
        AM13E_SERIAL_TX_MAX:remaining;
    const unsigned offset=cli_reply_offset;
    cli_reply_offset+=count; /* Publish before a possible EOT preemption. */
    if(!crsf_tx_start((const uint8_t *)cli_reply+offset,count))cli_reply_offset=offset;
}
void am13e_app_serial_cli_service_pending(void)
{
    if(!cli_active && cli_entry.requested){
        const uint32_t primask=__get_PRIMASK();
        __disable_irq();
        /* No waiting for UART idle with interrupts masked. Retry later
         * only while entry remains eligible; RX/capture cancels it.
         */
        (void)am13e_app_serial_cli_enter();
        __set_PRIMASK(primask);
    }
    if(!cli_active)return;
    if(cli_reply_length && crsf_tx.count==0U)cli_start_next_chunk();
    if(cli_rx.reboot){
        /* Original 00 ff reboot-to-Boot, deferred out of UART IRQ. */
        am13e_app_service_reset();
    }
    if(!cli_rx.ready||cli_reply_length!=0U||crsf_tx.count!=0U)
        return;
    if(!am13e_cli_line_take(&cli_rx,cli_reply,sizeof cli_reply))
        return;
    /* Preserve original execcmd() parser, including save/reset/play.
     * No stateful command executes in high-priority UART IRQ.
     */
    const int length=am13e_app_io_cli_line(cli_reply);
    if(length<0||length>(int)sizeof cli_reply){
        serial_fault();
    }
    cli_reply_length=(unsigned)length;
    cli_reply_offset=0U;
    if(cli_reply_length)cli_start_next_chunk();
}
void am13e_app_serial_cli_entry_poll(int high,int edge,int selected)
{
    if(cli_active)return;
    if(am13e_cli_entry_tick(&cli_entry,high,edge,selected))
        SCB->ICSR=SCB_ICSR_PENDSVSET_Msk;
}
#endif
#endif

void RX_HANDLER(void);

bool am13e_app_serial_rx_init(unsigned mode)
{
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    /* cfg.input_mode=0 still needs the explicitly selected CLI UART.
     * Binary frames on it are refused by the real source mode check.
     */
    if(mode==0U){cli_entry=(AM13E_CliEntry){0};return true;}
#endif
#ifdef PA26_SINGLE_WIRE
    /* CRSF transmitter links are full-duplex. RX-only CRSF remains a
     * separate valid selection; do not advertise single-pad full-duplex. */
    if(mode==5U)return false;
#endif
    if(__get_PRIMASK()!=1U || initialized ||
       (mode!=2U && mode!=3U && mode!=4U && mode!=5U && mode!=6U &&
        mode!=7U))return false;
    /* UCS are shared with I2C/SPI; board profile must assign a
     * DEDICATED instance that no other owner has initialized.
     */
    DL_UART_reset(RX_UC);
    DL_UART_enablePower(RX_UC);
    if(!DL_UART_isPowerEnabled(RX_UC))return false;
    DL_GPIO_initPeripheralInputFunctionFeatures(
        AM13E_BOARD_SERIAL_RX_PINCM,
        AM13E_BOARD_SERIAL_RX_PIN_FUNCTION,
        (mode==4U && AM13E_BOARD_SERIAL_SBUS_PIN_INVERT) ?
            DL_GPIO_INVERSION_ENABLE : DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE,DL_GPIO_WAKEUP_DISABLE);
    if(!DL_GPIO_isPeripheralConnected(AM13E_BOARD_SERIAL_RX_PINCM)||
       !DL_GPIO_isInputEnabled(AM13E_BOARD_SERIAL_RX_PINCM)||
       DL_GPIO_getPeripheralFunctionBits(AM13E_BOARD_SERIAL_RX_PINCM)!=
            AM13E_BOARD_SERIAL_RX_PIN_FUNCTION)
        return false;
    DL_UART_ClockConfig clk={
        .clockSel=DL_UART_CLOCK_BUSCLK,
        .divideRatio=DL_UART_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_setClockConfig(RX_UC,&clk);
    DL_UART_DIRECTION direction=DL_UART_DIRECTION_RX;
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
    if(mode==2U || mode==5U
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
       || mode==3U || mode==4U || mode==6U || mode==7U
#endif
#ifdef AM13E_BOARD_EXBUS_REPLY_ENABLED
       || mode==6U
#endif
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
       || mode==4U
#endif
       ) {
        /* Separate, board-reviewed 2-wire full-duplex TX/RX only.
         * Single-wire CRSF turnaround is NOT supported here.
         * Earlier initgpio() left the TX reservation Hi-Z.
         */
#ifndef PA26_SINGLE_WIRE
        DL_GPIO_initPeripheralOutputFunction(
            AM13E_BOARD_SERIAL_TX_PINCM,
            AM13E_BOARD_SERIAL_TX_PIN_FUNCTION);
        if(!DL_GPIO_isPeripheralConnected(
                 AM13E_BOARD_SERIAL_TX_PINCM)||
           DL_GPIO_getPeripheralFunctionBits(
                 AM13E_BOARD_SERIAL_TX_PINCM)!=
                 AM13E_BOARD_SERIAL_TX_PIN_FUNCTION)
            return false;
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
        DL_GPIO_setDataInversion(AM13E_BOARD_SERIAL_TX_PINCM,
            mode==4U && AM13E_BOARD_SBUS2_TX_PIN_INVERT ?
                DL_GPIO_INVERSION_ENABLE:DL_GPIO_INVERSION_DISABLE);
#endif
        direction=DL_UART_DIRECTION_TX_RX;
#endif
        crsf_tx_available=true;
    }
#endif
    DL_UART_Config config={
        .mode=DL_UART_MODE_NORMAL,
        .direction=direction,
        .flowControl=DL_UART_FLOW_CONTROL_NONE,
        .parity=mode==4U ? DL_UART_PARITY_EVEN : DL_UART_PARITY_NONE,
        .wordLength=DL_UART_WORD_LENGTH_8_BITS,
        .stopBits=mode==4U ? DL_UART_STOP_BITS_TWO :
                               DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(RX_UC,&config);
    DL_UART_configBaudRate(RX_UC,AM13E_BOARD_SERIAL_RX_BUSCLK_HZ,
                           mode==2U?SERIAL_BR:
                           mode==4U?100000U:
                           mode==5U?416666U:
                           mode==6U?125000U:115200U);
    DL_UART_enableFIFOs(RX_UC);
    DL_UART_setRXFIFOThreshold(RX_UC,DL_UART_RX_FIFO_LEVEL_NOT_EMPTY);
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
    if(crsf_tx_available)
        DL_UART_setTXFIFOThreshold(RX_UC,
                                   DL_UART_TX_FIFO_LEVEL_NOT_FULL);
#endif
    am13e_serial_frame_init(&rx_state,mode);
    receiver_selected=false;
    am13e_exbus_autobaud_init(&exbus_baud);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    am13e_cli_line_init(&cli_rx);
    cli_active=false;
    cli_entry=(AM13E_CliEntry){0};
    cli_reply_length=cli_reply_offset=0U;
#endif
    exbus_change_requested_tick=0U;
    exbus_change_pending=0U;
    DL_UART_clearInterruptStatus(RX_UC,
                  DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
    DL_UART_enableInterrupt(RX_UC,
                  DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
    if(mode==4U && !sbus2_timer_init())return false;
#endif
    DL_UART_enable(RX_UC);
    initialized=1U;
    NVIC_SetPriority(RX_IRQ,1U);
    NVIC_EnableIRQ(RX_IRQ);
    return true;
}
void am13e_app_serial_rx_systick(void)
{
    if(!initialized)return;
    ++serial_rx_tick;
#ifdef PA26_SINGLE_WIRE
    if(wire_tx_active && (uint32_t)(serial_rx_tick-wire_started)>=512U){
        wire_abort_pending=true;SCB->ICSR=SCB_ICSR_PENDSVSET_Msk;
    }
#endif
    /* The selected UART is configured in preemptible PendSV ONLY;
     * a 4-tick guard avoids a write while a character is in flight.
     * Periodic 8-tick retries remain bounded and nonblocking.
     */
    if(rx_state.mode==6U && exbus_change_pending &&
       (uint32_t)(serial_rx_tick-exbus_change_requested_tick)>=4U &&
       (serial_rx_tick&7U)==0U)
        SCB->ICSR=SCB_ICSR_PENDSVSET_Msk;
}
bool am13e_app_serial_rx_irq_ready(void)
{
    return initialized &&
        NVIC_GetVector(RX_IRQ)==
             ((uint32_t)(uintptr_t)RX_HANDLER|1U) &&
        NVIC_GetEnableIRQ(RX_IRQ) &&
        NVIC_GetPriority(RX_IRQ)==1U;
}

void RX_HANDLER(void)
{
    if(!initialized){
        serial_fault();
    }
    /* Extract error + byte in ONE read: DL_UART_getErrorStatus()
     * and DL_UART_receiveData() both access RXDATA and combining
     * them would consume two bytes from the FIFO.
     */
    for(unsigned budget=0U;budget<64U;++budget){
        const DL_UART_IIDX reason=DL_UART_getPendingInterrupt(RX_UC);
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
        if(crsf_tx_available && reason==DL_UART_IIDX_EOT_DONE){
            /* Shift register finished, not merely FIFO-filled. */
            if(!am13e_serial_tx_queue_waiting_eot(&crsf_tx)){
                serial_fault();
            }
            DL_UART_disableInterrupt(RX_UC,
                DL_UART_INTERRUPT_EOT_DONE|DL_UART_INTERRUPT_TX);
            am13e_serial_tx_queue_eot(&crsf_tx);
#ifdef PA26_SINGLE_WIRE
            wire_rx();
#endif
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
            sbus2_release_rx();
#endif
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
            if(cli_active){
                if(cli_reply_offset<cli_reply_length)
                    cli_start_next_chunk();
                else cli_reply_length=cli_reply_offset=0U;
            }
#endif
            continue;
        }
        if(crsf_tx_available && reason==DL_UART_IIDX_TX){
            crsf_tx_fill_fifo();
            continue;
        }
#endif
        if(reason==DL_UART_IIDX_NO_INTERRUPT &&
           DL_UART_isRXFIFOEmpty(RX_UC))break;
        if(reason==DL_UART_IIDX_OVERRUN_ERROR ||
           reason==DL_UART_IIDX_PARITY_ERROR ||
           reason==DL_UART_IIDX_FRAMING_ERROR ||
           reason==DL_UART_IIDX_NOISE_ERROR ||
           reason==DL_UART_IIDX_BREAK_ERROR){
            am13e_serial_frame_abort(&rx_state);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
            if(cli_active)am13e_cli_line_abort(&cli_rx);
#endif
            DL_UART_clearInterruptStatus(RX_UC,RX_IRQ_ERRORS);
            exbus_request_retry();
        }
        if(DL_UART_isRXFIFOEmpty(RX_UC))break;
        const uint32_t raw=RX_UC->uart->RXDATA;
        if(!receiver_selected && am13e_app_io_input_mode()>=2U
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
           && !cli_active
#endif
           ) {
            am13e_app_io_watchdog_arm();
            receiver_selected=true;
        }
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
        /* Rel17 has selected serial input before it validates a frame.
         * Even malformed received bytes must cancel deferred CLI entry.
         */
        if(!cli_active)am13e_cli_entry_select_receiver(&cli_entry);
#endif
        if((raw&RX_ERRORS)!=0U){
            am13e_serial_frame_abort(&rx_state);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
            if(cli_active)am13e_cli_line_abort(&cli_rx);
#endif
            exbus_request_retry();
            continue;
        }
        /* All bytes captured at the OLD baud are discarded until
         * the queued PendSV configuration actually completes.
         */
        if(rx_state.mode==6U && exbus_change_pending)continue;
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
        if(cli_active){
            /* Legacy half-duplex RX mute while the response is sent.
             * This is a conditional two-wire software equivalent.
             */
            if(cli_reply_length!=0U||crsf_tx.count!=0U)continue;
            const int ready=am13e_cli_line_byte(&cli_rx,
                (uint8_t)(raw&UNICOMMUART_RXDATA_DATA_MASK));
            if(ready==1||ready==2)SCB->ICSR=SCB_ICSR_PENDSVSET_Msk;
            continue;
        }
#endif
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
        if(sbus2_active)continue; /* Original reply-window RX mute. */
#endif
        uint8_t bytes[AM13E_SERIAL_MAX_FRAME];
        unsigned len=0U;
        /* Original USART DMA received a complete packet at IDLE.
         * Here an incomplete packet is dropped only when the
         * previous FIFO byte was >=4 16kHz ticks earlier (>=250us).
         * This is a provisional software boundary, NOT a claim
         * about actual EXBUS/SUMD inter-frame timing on silicon.
         */
        const uint32_t before_errors=rx_state.frame_errors;
        am13e_serial_frame_gap(&rx_state,serial_rx_tick,4U);
        if(rx_state.mode==6U && rx_state.frame_errors!=before_errors){
            exbus_request_retry();
            continue;
        }
        const bool complete=am13e_serial_frame_byte(&rx_state,
                (uint8_t)(raw&UNICOMMUART_RXDATA_DATA_MASK),
                bytes,sizeof bytes,&len);
        if(rx_state.mode==6U && rx_state.frame_errors!=before_errors){
            exbus_request_retry();
            continue;
        }
        if(complete){
            if(rx_state.mode==2U){
                uint8_t reply[12];unsigned reply_len=0U;
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
                if(!crsf_tx_available || crsf_tx.count!=0U)
                    continue; /* No ACK can be queued while busy. */
#endif
                const int valid=am13e_app_io_serial_frame(
                    bytes,len,reply,sizeof reply,&reply_len);
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
                if(valid && reply_len!=0U)
                    crsf_tx_start(reply,reply_len);
#else
                (void)valid;
#endif
            }
            else if(rx_state.mode==3U)
                (void)am13e_app_io_ibus_frame(bytes,len);
            else if(rx_state.mode==4U){
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
                const uint16_t received=sbus2_now();
#endif
                (void)am13e_app_io_sbus_frame(bytes,len);
#ifdef AM13E_BOARD_SBUS2_TIMED_REPLY
                if(am13e_app_io_input_mode()==4U && len==25U &&
                   bytes[0]==0x0fU && (bytes[24]&0x0fU)==4U)
                    sbus2_begin(received);
#endif
            }
            else if(rx_state.mode==6U){
                const int valid=am13e_app_io_exbus_frame(bytes,len);
#ifdef AM13E_BOARD_EXBUS_REPLY_ENABLED
                if(valid&&bytes[4]==0x3aU&&bytes[5]==0U &&
                   crsf_tx_available&&crsf_tx.count==0U){
                    /* This uses the ACTUAL original EXBUS payload/CRC,
                     * with a BOARD-OWNED stable UID. Request ID echo.
                     * Two-wire external electrical topology is
                     * conditional and NOT qualified on Reference.
                     */
                    uint8_t reply[AM13E_SERIAL_TX_MAX];
                    const unsigned response=am13e_app_io_exbus_telemetry(
                        bytes[3],AM13E_BOARD_EXBUS_STABLE_DEVICE_ID,
                        reply,sizeof reply);
                    if(response)crsf_tx_start(reply,response);
                }
#endif
                if(valid){
                    /* Upstream n=0 ONLY when 0x31 channel data
                     * arrives. CRC-valid unknown and telemetry
                     * frames do not consume the retry budget.
                     */
                    am13e_exbus_autobaud_valid(
                        &exbus_baud,bytes[4]==0x31U);
                } else {
                    exbus_request_retry();
                }
            }
            else if(rx_state.mode==7U)
                (void)am13e_app_io_hott_frame(bytes,len);
            else {
                const int valid=am13e_app_io_crsf_frame(bytes,len);
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
                /* Do not execute 0x2d Write (or Save/Reset) if the
                 * previous reply is still on the wire. Executing
                 * execcrsfcmd() and THEN rejecting its ACK due to a
                 * busy TX queue could mutate cfg without a response.
                 * The TX queue is owned by this single UC IRQ.
                 */
                if(valid && crsf_tx_available && crsf_tx.count==0U &&
                   !crsf_cmd.busy &&
                   (bytes[2]==0x28U || bytes[2]==0x2cU ||
                    bytes[2]==0x2dU) &&
                   am13e_crsf_deferred_begin(&crsf_cmd,bytes,len)){
                    /* NVIC dispatches PendSV only after UART ISR exit.
                     * Never execute stateful Rel17 command here.
                     */
                    SCB->ICSR=SCB_ICSR_PENDSVSET_Msk;
                }
#else
                (void)valid;
#endif
            }
        }
    }
}
#else
/* Default Reference provides no UART-backed receiver and exports
 * no UART function or interrupt symbol. Keep strict ISO C happy
 * without a fake-success serial implementation.
 */
enum { AM13E_REFERENCE_SERIAL_RX_DISABLED = 0 };
#endif /* board-selected UART input only */
