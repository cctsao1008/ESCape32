/* Native optional TI AM13E UNICOMM telemetry transport for ORIGINAL
 * src/telem.c handlers and scheduler. Never linked to active Reference
 * UART unless an explicit board electrical/profile selection is made.
 *
 * This covers 2-WIRE UART source/Host/ARM transport only.
 * Original single-wire half-duplex bus turnarounds, S.Port PHY and
 * HoTT real-time slots are NOT hardware-qualified by this module.
 */
#include "telem_backend.h"
#ifdef AM13E_BOARD_TELEM_UART_CONFIGURED
#include "telem_frame_rx.h"
#include "serial_tx_queue.h"
#include "telem_mode_plan.h"
#include "board_reference_io.h"
#include "board_configuration.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_unicommuart.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#if AM13E_BOARD_TELEM_UART_CONFIGURED != 1 || \
    !defined(AM13E_BOARD_TELEM_UC_INDEX) || \
    !defined(AM13E_BOARD_TELEM_RX_PINCM) || \
    !defined(AM13E_BOARD_TELEM_TX_PINCM) || \
    !defined(AM13E_BOARD_TELEM_RX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_TELEM_TX_PIN_FUNCTION) || \
    !defined(AM13E_BOARD_TELEM_BUSCLK_HZ) || \
    !defined(AM13E_BOARD_TELEM_TWO_WIRE)
#error "Telemetry UNICOMM needs reviewed independent RX/TX pads, UC and bus clock"
#endif
/* SDK PF encoding admits documented UART routes above function7;
 * legality of the selected UC/pad still belongs to board qualification. */
_Static_assert(AM13E_BOARD_TELEM_TWO_WIRE==1U &&
               AM13E_BOARD_TELEM_RX_PINCM>0U &&
               AM13E_BOARD_TELEM_RX_PINCM<107U &&
               AM13E_BOARD_TELEM_TX_PINCM>0U &&
               AM13E_BOARD_TELEM_TX_PINCM<107U &&
               AM13E_BOARD_TELEM_RX_PINCM!=AM13E_BOARD_TELEM_TX_PINCM &&
               AM13E_BOARD_TELEM_RX_PIN_FUNCTION>0U &&
               AM13E_BOARD_TELEM_RX_PIN_FUNCTION<=IOMUX_PINCM_PF_MASK &&
               AM13E_BOARD_TELEM_TX_PIN_FUNCTION>0U &&
               AM13E_BOARD_TELEM_TX_PIN_FUNCTION<=IOMUX_PINCM_PF_MASK &&
               AM13E_BOARD_TELEM_BUSCLK_HZ>=2000000U &&
               AM13E_BOARD_TELEM_BUSCLK_HZ<=200000000U,
               "Invalid or overlapping UNICOMM telemetry board profile");
_Static_assert(AM13E_BOARD_TELEM_RX_PINCM!=IOMUX_PINCM_PA26 &&
               AM13E_BOARD_TELEM_TX_PINCM!=IOMUX_PINCM_PA26 &&
               AM13E_BOARD_TELEM_RX_PINCM!=IOMUX_PINCM_PB13 &&
               AM13E_BOARD_TELEM_TX_PINCM!=IOMUX_PINCM_PB13 &&
               AM13E_BOARD_TELEM_RX_PINCM!=IOMUX_PINCM_PB15 &&
               AM13E_BOARD_TELEM_TX_PINCM!=IOMUX_PINCM_PB15,
               "Telemetry cannot claim motor enable, input or fault pins");
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
_Static_assert(AM13E_BOARD_TELEM_UC_INDEX!=AM13E_BOARD_SERIAL_RX_UC_INDEX &&
               AM13E_BOARD_TELEM_RX_PINCM!=AM13E_BOARD_SERIAL_RX_PINCM &&
               AM13E_BOARD_TELEM_TX_PINCM!=AM13E_BOARD_SERIAL_RX_PINCM &&
               AM13E_BOARD_TELEM_RX_PINCM!=AM13E_BOARD_SERIAL_TX_PINCM &&
               AM13E_BOARD_TELEM_TX_PINCM!=AM13E_BOARD_SERIAL_TX_PINCM,
               "Serial input and telemetry require different hardware instances/pins");
#endif
#if AM13E_BOARD_TELEM_UC_INDEX==0
#define TEL_UC UC0_INST_PTR
#define TEL_IRQ UC0_INT_IRQn
#define TEL_HANDLER UC0_IRQHandler
#elif AM13E_BOARD_TELEM_UC_INDEX==1
#define TEL_UC UC1_INST_PTR
#define TEL_IRQ UC1_INT_IRQn
#define TEL_HANDLER UC1_IRQHandler
#elif AM13E_BOARD_TELEM_UC_INDEX==2
#define TEL_UC UC2_INST_PTR
#define TEL_IRQ UC2_INT_IRQn
#define TEL_HANDLER UC2_IRQHandler
#elif AM13E_BOARD_TELEM_UC_INDEX==3
#define TEL_UC UC3_INST_PTR
#define TEL_IRQ UC3_INT_IRQn
#define TEL_HANDLER UC3_IRQHandler
#elif AM13E_BOARD_TELEM_UC_INDEX==4
#define TEL_UC UC4_INST_PTR
#define TEL_IRQ UC4_INT_IRQn
#define TEL_HANDLER UC4_IRQHandler
#elif AM13E_BOARD_TELEM_UC_INDEX==5
#define TEL_UC UC5_INST_PTR
#define TEL_IRQ UC5_INT_IRQn
#define TEL_HANDLER UC5_IRQHandler
#else
#error "Telemetry UC selection must be UNICOMM0..5"
#endif

#define BYTE_ERRORS (UNICOMMUART_RXDATA_FRMERR_SET | \
                     UNICOMMUART_RXDATA_PARERR_SET | \
                     UNICOMMUART_RXDATA_BRKERR_SET | \
                     UNICOMMUART_RXDATA_NERR_SET)
#define INT_ERRORS (DL_UART_INTERRUPT_OVERRUN_ERROR | \
                    DL_UART_INTERRUPT_FRAMING_ERROR | \
                    DL_UART_INTERRUPT_PARITY_ERROR | \
                    DL_UART_INTERRUPT_NOISE_ERROR)
static AM13E_TelemFrameRx rx;
static AM13E_SerialTxQueue tx;
static char *source_buffer;
static unsigned source_capacity;
static unsigned active_mode,initialized;
static volatile uint32_t ticker;
static uint32_t previous_rx_tick;
static bool seen_rx_tick,paused_rx;
static bool tx_is_deferred_byte,tx_final_deferred_byte;
extern void hard_fault_handler(void);
static void transport_fault(void)
{
    __disable_irq();
    hard_fault_handler();
    for(;;){__NOP();}
}
static void tx_refill(void)
{
    for(unsigned budget=0U;budget<AM13E_SERIAL_TX_MAX;++budget){
        if(DL_UART_isTXFIFOFull(TEL_UC))break;
        uint8_t data=0U;
        if(!am13e_serial_tx_queue_next(&tx,&data))break;
        DL_UART_transmitData(TEL_UC,data);
    }
    if(am13e_serial_tx_queue_waiting_eot(&tx))
        DL_UART_disableInterrupt(TEL_UC,DL_UART_INTERRUPT_TX);
}
static void tx_begin(const uint8_t *p,unsigned n,
                     bool deferred_byte,bool last)
{
    if(!initialized||!am13e_serial_tx_queue_begin(&tx,p,n))
        transport_fault(); /* never report an unsent ACK as successful */
    tx_is_deferred_byte=deferred_byte;
    tx_final_deferred_byte=last;
    DL_UART_clearInterruptStatus(TEL_UC,
          DL_UART_INTERRUPT_TX|DL_UART_INTERRUPT_EOT_DONE);
    DL_UART_enableInterrupt(TEL_UC,DL_UART_INTERRUPT_EOT_DONE);
    tx_refill();
    if(!am13e_serial_tx_queue_waiting_eot(&tx))
        DL_UART_enableInterrupt(TEL_UC,DL_UART_INTERRUPT_TX);
}
void am13e_telem_hw_init(int mode,char *buffer,
                         unsigned capacity,bool rx_required)
{
    AM13E_TelemModePlan p;
    if(__get_PRIMASK()!=1U||initialized||!buffer||capacity<64U||
       !am13e_telem_mode_plan(mode,&p)||
       rx_required!=(p.rx_protocol!=0U))
        transport_fault();
    /* UART owner is selected by the board; no phony UC/pin mapping
     * follows from syntactically valid synthetic test definitions.
     */
    DL_UART_reset(TEL_UC);
    DL_UART_enablePower(TEL_UC);
    if(!DL_UART_isPowerEnabled(TEL_UC))transport_fault();
    const DL_GPIO_INVERSION invert=p.inverted_line?
        DL_GPIO_INVERSION_ENABLE:DL_GPIO_INVERSION_DISABLE;
    DL_GPIO_initPeripheralInputFunctionFeatures(
       AM13E_BOARD_TELEM_RX_PINCM,AM13E_BOARD_TELEM_RX_PIN_FUNCTION,
       invert,DL_GPIO_RESISTOR_NONE,DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralOutputFunctionFeatures(
       AM13E_BOARD_TELEM_TX_PINCM,AM13E_BOARD_TELEM_TX_PIN_FUNCTION,
       invert,DL_GPIO_RESISTOR_NONE,
       DL_GPIO_DRIVE_STRENGTH_LOW,DL_GPIO_HIZ_DISABLE);
    if(!DL_GPIO_isPeripheralConnected(AM13E_BOARD_TELEM_RX_PINCM)||
       !DL_GPIO_isPeripheralConnected(AM13E_BOARD_TELEM_TX_PINCM)||
       !DL_GPIO_isInputEnabled(AM13E_BOARD_TELEM_RX_PINCM)||
       DL_GPIO_getPeripheralFunctionBits(AM13E_BOARD_TELEM_RX_PINCM)!=
            AM13E_BOARD_TELEM_RX_PIN_FUNCTION||
       DL_GPIO_getPeripheralFunctionBits(AM13E_BOARD_TELEM_TX_PINCM)!=
            AM13E_BOARD_TELEM_TX_PIN_FUNCTION)
        transport_fault();
    DL_UART_ClockConfig clock={
       .clockSel=DL_UART_CLOCK_BUSCLK,
       .divideRatio=DL_UART_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_setClockConfig(TEL_UC,&clock);
    DL_UART_Config config={
       .mode=DL_UART_MODE_NORMAL,
       .direction=DL_UART_DIRECTION_TX_RX,
       .flowControl=DL_UART_FLOW_CONTROL_NONE,
       .parity=DL_UART_PARITY_NONE,
       .wordLength=DL_UART_WORD_LENGTH_8_BITS,
       .stopBits=DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(TEL_UC,&config);
    DL_UART_configBaudRate(TEL_UC,AM13E_BOARD_TELEM_BUSCLK_HZ,p.baud_rate);
    DL_UART_enableFIFOs(TEL_UC);
    DL_UART_setRXFIFOThreshold(TEL_UC,DL_UART_RX_FIFO_LEVEL_NOT_EMPTY);
    DL_UART_setTXFIFOThreshold(TEL_UC,DL_UART_TX_FIFO_LEVEL_NOT_FULL);
    source_buffer=buffer;
    source_capacity=capacity;
    active_mode=(unsigned)mode;
    am13e_telem_frame_init(&rx,active_mode);
    paused_rx=false;
    ticker=0U;
    seen_rx_tick=false;
    DL_UART_clearInterruptStatus(TEL_UC,
                                 DL_UART_INTERRUPT_RX|INT_ERRORS);
    DL_UART_enableInterrupt(TEL_UC,DL_UART_INTERRUPT_RX|INT_ERRORS);
    DL_UART_enable(TEL_UC);
    initialized=1U;
    NVIC_SetPriority(TEL_IRQ,1U);
    NVIC_EnableIRQ(TEL_IRQ);
}
bool am13e_telem_hw_tx_busy(void){return tx.count!=0U;}
void am13e_telem_hw_tx_start(const char *p,unsigned bytes)
{
    if(!p||bytes==0U||bytes>AM13E_SERIAL_TX_MAX)transport_fault();
    tx_begin((const uint8_t *)p,bytes,false,false);
}
void am13e_telem_hw_tx_byte(uint8_t b,bool last)
{
    tx_begin(&b,1U,true,last);
}
void am13e_telem_hw_pause_rx(void){paused_rx=true;}
void am13e_telem_hw_systick(void){if(initialized)++ticker;}
void TEL_HANDLER(void); /* installed M33 vector, required by irq_ready */
bool am13e_telem_hw_irq_ready(void)
{
    return initialized&&NVIC_GetEnableIRQ(TEL_IRQ)&&
           NVIC_GetPriority(TEL_IRQ)==1U&&
           NVIC_GetVector(TEL_IRQ)==
               ((uint32_t)(uintptr_t)TEL_HANDLER|1U);
}
void TEL_HANDLER(void)
{
    if(!initialized)transport_fault();
    for(unsigned budget=0U;budget<64U;++budget){
        const DL_UART_IIDX reason=DL_UART_getPendingInterrupt(TEL_UC);
        if(reason==DL_UART_IIDX_EOT_DONE){
            if(!am13e_serial_tx_queue_waiting_eot(&tx))
                transport_fault();
            const bool byte=tx_is_deferred_byte;
            const bool last=tx_final_deferred_byte;
            DL_UART_disableInterrupt(TEL_UC,
                           DL_UART_INTERRUPT_EOT_DONE|DL_UART_INTERRUPT_TX);
            am13e_serial_tx_queue_eot(&tx);
            tx_is_deferred_byte=false;
            tx_final_deferred_byte=false;
            if(byte){
                if(last)paused_rx=false; /* HoTT final EOT */
            }else am13e_telem_on_tx_done(); /* original queued-tail */
            continue;
        }
        if(reason==DL_UART_IIDX_TX){tx_refill();continue;}
        if(reason==DL_UART_IIDX_NO_INTERRUPT&&
           DL_UART_isRXFIFOEmpty(TEL_UC))break;
        if(reason==DL_UART_IIDX_OVERRUN_ERROR||
           reason==DL_UART_IIDX_PARITY_ERROR||
           reason==DL_UART_IIDX_FRAMING_ERROR||
           reason==DL_UART_IIDX_NOISE_ERROR||
           reason==DL_UART_IIDX_BREAK_ERROR){
            am13e_telem_frame_abort(&rx);
            DL_UART_clearInterruptStatus(TEL_UC,INT_ERRORS);
        }
        if(DL_UART_isRXFIFOEmpty(TEL_UC))break;
        const uint32_t raw=TEL_UC->uart->RXDATA; /* ONE FIFO pop */
        if((raw&BYTE_ERRORS)!=0U){
            am13e_telem_frame_abort(&rx);
            continue;
        }
        if(paused_rx||active_mode==0U||active_mode==1U||
           active_mode==4U)continue;
        if(seen_rx_tick)
            am13e_telem_frame_gap(&rx,
                (uint32_t)(ticker-previous_rx_tick),8U);
        previous_rx_tick=ticker;
        seen_rx_tick=true;
        uint8_t request[4]={0};unsigned n=0U;
        if(am13e_telem_frame_push(&rx,
                     (uint8_t)(raw&UNICOMMUART_RXDATA_DATA_MASK),
                     request,sizeof request,&n)){
            if(n>source_capacity||tx.count!=0U)
                continue; /* never overwrite in-flight source storage */
            for(unsigned i=0U;i<n;++i)source_buffer[i]=(char)request[i];
            const int reply=am13e_telem_on_rx_frame(n);
            if(reply>0&&reply<=(int)source_capacity)
                am13e_telem_hw_tx_start(source_buffer,(unsigned)reply);
            else if(reply>(int)source_capacity)
                transport_fault();
            /* reply==-1: original HoTT 5ms wait then bytewise TX
             * from sendtelem() in PendSV. No UART ISR busy wait.
             */
        }
    }
}
#else
/* No actual Reference telemetry UART and no silent fake provider. */
enum { AM13E_REFERENCE_TELEM_UART_DISABLED = 0 };
#endif
