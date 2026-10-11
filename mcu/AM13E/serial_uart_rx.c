/* Native TI AM13E UNICOMM IRQ/FIFO RX for Rel17 input_mode 3/4/5.
 * Reference has no assigned UART, so this implementation is selected
 * ONLY by explicit AM13E_BOARD_SERIAL_RX_CONFIGURED board definitions.
 * Optional board-selected 2-wire CRSF command TX uses UNICOMM FIFO,
 * TX interrupt and EOT; NO DMA, single-wire half-duplex bus, SBUS2
 * slot timing or verified physical pin mapping is implied.
 */
#include "serial_uart_rx.h"
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#include "serial_frame_rx.h"
#include "exbus_autobaud.h"
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
#include "serial_tx_queue.h"
#include "crsf_deferred_command.h"
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
#include "cli_line_rx.h"
#endif
#endif
#include "io_backend.h"
#if defined(AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED) && \
    (!defined(AM13E_BOARD_SERIAL_TX_CONFIGURED) || \
     AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED!=1)
#error "CLI requires board-selected two-wire TX and explicit entry qualification"
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
#include <soc.h>
#include <dl_gpio.h>
#include <dl_unicommuart.h>
#include <stdint.h>

#if AM13E_BOARD_SERIAL_RX_CONFIGURED != 1 || \
    !defined(AM13E_BOARD_SERIAL_RX_UC_INDEX) || \
    !defined(AM13E_BOARD_SERIAL_RX_PINCM) || \
    !defined(AM13E_BOARD_SERIAL_RX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_SERIAL_RX_BUSCLK_HZ) || \
    !defined(AM13E_BOARD_SERIAL_SBUS_PIN_INVERT)
#error "Board UART RX requires dedicated UC, RX pin/function, verified BUSCLK"
#endif
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM>0U &&
               AM13E_BOARD_SERIAL_RX_PINCM<107U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION>0U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION<8U &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ>=UINT32_C(2000000) &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ<=UINT32_C(200000000) &&
               AM13E_BOARD_SERIAL_SBUS_PIN_INVERT<=1U,
               "Native UART RX board route/function/clock invalid");
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PB14 &&
               AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PB15 &&
               AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PB13 &&
               AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PA6 &&
               AM13E_BOARD_SERIAL_RX_PINCM!=IOMUX_PINCM_PA28 &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_UH_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_UL_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_VH_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_VL_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_WH_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_IO_PWM_WL_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP0_HP_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP0_HN_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP1_HP_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP1_HN_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP3_HP_PINCM &&
               AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BEMF_CMP3_HN_PINCM &&
               (AM13E_BOARD_SERIAL_TX_PINCM==0U ||
                AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_SERIAL_TX_PINCM) &&
               (AM13E_BOARD_OC_GPIO_PINCM==0U ||
                AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_OC_GPIO_PINCM) &&
               (AM13E_BOARD_CURRENT_SENSE_PINCM==0U ||
                AM13E_BOARD_SERIAL_RX_PINCM!=AM13E_BOARD_CURRENT_SENSE_PINCM),
               "Native serial RX pin conflicts with board-reserved IO");

#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
#if AM13E_BOARD_SERIAL_TX_CONFIGURED != 1 || \
    !defined(AM13E_BOARD_SERIAL_TX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_SERIAL_TX_TWO_WIRE)
#error "CRSF TX requires explicit TX pin function and 2-wire board topology"
#endif
_Static_assert(AM13E_BOARD_SERIAL_TX_TWO_WIRE==1U &&
               AM13E_BOARD_SERIAL_TX_PINCM>0U &&
               AM13E_BOARD_SERIAL_TX_PINCM<64U &&
               AM13E_BOARD_SERIAL_TX_PIN_FUNCTION>0U &&
               AM13E_BOARD_SERIAL_TX_PIN_FUNCTION<8U &&
               AM13E_BOARD_SERIAL_TX_PINCM!=AM13E_BOARD_SERIAL_RX_PINCM,
               "CRSF TX must own a distinct board-assigned two-wire pin");
#endif

#if AM13E_BOARD_SERIAL_RX_UC_INDEX==0
#define RX_UC UC0_INST_PTR
#define RX_IRQ UC0_INT_IRQn
#define RX_HANDLER UC0_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==1
#define RX_UC UC1_INST_PTR
#define RX_IRQ UC1_INT_IRQn
#define RX_HANDLER UC1_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==2
#define RX_UC UC2_INST_PTR
#define RX_IRQ UC2_INT_IRQn
#define RX_HANDLER UC2_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==3
#define RX_UC UC3_INST_PTR
#define RX_IRQ UC3_INT_IRQn
#define RX_HANDLER UC3_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==4
#define RX_UC UC4_INST_PTR
#define RX_IRQ UC4_INT_IRQn
#define RX_HANDLER UC4_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==5
#define RX_UC UC5_INST_PTR
#define RX_IRQ UC5_INT_IRQn
#define RX_HANDLER UC5_IRQHandler
#else
#error "Select AM13E UNICOMM0..UNICOMM5, with no other owner"
#endif

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
static char cli_reply[AM13E_CLI_LINE_BYTES];
static unsigned cli_reply_length,cli_reply_offset;
#endif

/* Original CRSF command execution is deferred to PendSV.
 * Queue writes begin from PendSV, while TX/EOT drain in UART IRQ.
 */
/* PendSV calls this worker; both worker and UART IRQ use
 * the same bounded TX queue owned by this UNICOMM instance.
 */
static void crsf_tx_start(const uint8_t *p,unsigned n);
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
static void crsf_tx_start(const uint8_t *p,unsigned n)
{
    if(!crsf_tx_available ||
       !am13e_serial_tx_queue_begin(&crsf_tx,p,n))return;
    DL_UART_clearInterruptStatus(RX_UC,
                                 DL_UART_INTERRUPT_TX |
                                 DL_UART_INTERRUPT_EOT_DONE);
    DL_UART_enableInterrupt(RX_UC,DL_UART_INTERRUPT_EOT_DONE);
    crsf_tx_fill_fifo();
    if(!am13e_serial_tx_queue_waiting_eot(&crsf_tx))
        DL_UART_enableInterrupt(RX_UC,DL_UART_INTERRUPT_TX);
}
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
/* Physical entry requires a board-verified original 1s idle-low signal.
 * Never selected or auto-inferred by the PB14 Reference profile.
 * Selected mode2 binary serial and CLI share the UART, not a parser.
 */
bool am13e_app_serial_cli_enter(void)
{
    if(!initialized||cli_active||rx_state.mode!=2U||
       __get_PRIMASK()!=1U||!crsf_tx_available||
       crsf_tx.count!=0U||crsf_cmd.busy||
       !DL_UART_isRXFIFOEmpty(RX_UC)||DL_UART_isBusy(RX_UC))
        return false;
    DL_UART_disable(RX_UC);
    DL_UART_configBaudRate(RX_UC,AM13E_BOARD_SERIAL_RX_BUSCLK_HZ,38400U);
    DL_UART_enable(RX_UC);
    am13e_serial_frame_abort(&rx_state);
    am13e_cli_line_init(&cli_rx);
    cli_reply_length=cli_reply_offset=0U;
    cli_active=true;
    return true;
}
static void cli_start_next_chunk(void)
{
    if(cli_reply_offset>=cli_reply_length)return;
    const unsigned remaining=cli_reply_length-cli_reply_offset;
    const unsigned count=remaining>AM13E_SERIAL_TX_MAX?
        AM13E_SERIAL_TX_MAX:remaining;
    crsf_tx_start((const uint8_t *)cli_reply+cli_reply_offset,count);
    cli_reply_offset+=count;
}
void am13e_app_serial_cli_service_pending(void)
{
    if(!cli_active)return;
    if(cli_rx.reboot){
        /* Original 00 ff reboot-to-Boot, deferred out of UART IRQ. */
        NVIC_SystemReset();
        for(;;){__NOP();}
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
        __disable_irq();for(;;){__NOP();}
    }
    cli_reply_length=(unsigned)length;
    cli_reply_offset=0U;
    if(cli_reply_length)cli_start_next_chunk();
}
#endif
#endif

void RX_HANDLER(void);

bool am13e_app_serial_rx_init(unsigned mode)
{
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
#ifdef AM13E_BOARD_EXBUS_REPLY_ENABLED
       || mode==6U
#endif
       ) {
        /* Separate, board-reviewed 2-wire full-duplex TX/RX only.
         * Single-wire CRSF turnaround is NOT supported here.
         * Earlier initgpio() left the TX reservation Hi-Z.
         */
        DL_GPIO_initPeripheralOutputFunction(
            AM13E_BOARD_SERIAL_TX_PINCM,
            AM13E_BOARD_SERIAL_TX_PIN_FUNCTION);
        if(!DL_GPIO_isPeripheralConnected(
                 AM13E_BOARD_SERIAL_TX_PINCM)||
           DL_GPIO_getPeripheralFunctionBits(
                 AM13E_BOARD_SERIAL_TX_PINCM)!=
                 AM13E_BOARD_SERIAL_TX_PIN_FUNCTION)
            return false;
        direction=DL_UART_DIRECTION_TX_RX;
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
                           mode==2U?460800U:
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
    am13e_exbus_autobaud_init(&exbus_baud);
#ifdef AM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED
    am13e_cli_line_init(&cli_rx);
    cli_active=false;
    cli_reply_length=cli_reply_offset=0U;
#endif
    exbus_change_requested_tick=0U;
    exbus_change_pending=0U;
    DL_UART_clearInterruptStatus(RX_UC,
                  DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
    DL_UART_enableInterrupt(RX_UC,
                  DL_UART_INTERRUPT_RX|RX_IRQ_ERRORS);
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
        __disable_irq();
        for(;;) { __NOP(); }
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
                __disable_irq();
                for(;;){__NOP();}
            }
            DL_UART_disableInterrupt(RX_UC,
                DL_UART_INTERRUPT_EOT_DONE|DL_UART_INTERRUPT_TX);
            am13e_serial_tx_queue_eot(&crsf_tx);
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
            else if(rx_state.mode==4U)
                (void)am13e_app_io_sbus_frame(bytes,len);
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
