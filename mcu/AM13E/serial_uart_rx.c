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
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
#include "serial_tx_queue.h"
#include "crsf_deferred_command.h"
#endif
#include "io_backend.h"
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
#ifdef AM13E_BOARD_SERIAL_TX_CONFIGURED
static AM13E_SerialTxQueue crsf_tx;
static AM13E_CrsfDeferredCommand crsf_cmd;
static bool crsf_tx_available;

/* Called solely from this UNICOMM IRQ. Nonblocking, single-packet
 * transaction: queue copy finishes before TX IRQ can drain it.
 */
/* PendSV calls this worker; both worker and UART IRQ use
 * the same bounded TX queue owned by this UNICOMM instance.
 */
static void crsf_tx_start(const uint8_t *p,unsigned n);

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
#endif

void RX_HANDLER(void);

bool am13e_app_serial_rx_init(unsigned mode)
{
    if(__get_PRIMASK()!=1U || initialized ||
       (mode!=3U && mode!=4U && mode!=5U && mode!=6U &&
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
    if(mode==5U) {
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
            DL_UART_clearInterruptStatus(RX_UC,RX_IRQ_ERRORS);
        }
        if(DL_UART_isRXFIFOEmpty(RX_UC))break;
        const uint32_t raw=RX_UC->uart->RXDATA;
        if((raw&RX_ERRORS)!=0U){
            am13e_serial_frame_abort(&rx_state);
            continue;
        }
        uint8_t bytes[AM13E_SERIAL_MAX_FRAME];
        unsigned len=0U;
        if(am13e_serial_frame_byte(&rx_state,
                (uint8_t)(raw&UNICOMMUART_RXDATA_DATA_MASK),
                bytes,sizeof bytes,&len)){
            if(rx_state.mode==3U)
                (void)am13e_app_io_ibus_frame(bytes,len);
            else if(rx_state.mode==4U)
                (void)am13e_app_io_sbus_frame(bytes,len);
            else if(rx_state.mode==6U)
                (void)am13e_app_io_exbus_frame(bytes,len);
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
