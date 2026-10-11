/* Native TI AM13E UNICOMM IRQ/FIFO RX for Rel17 input_mode 3/4.
 * Reference has no assigned UART, so this implementation is selected
 * ONLY by explicit AM13E_BOARD_SERIAL_RX_CONFIGURED board definitions.
 * No DMA/telemetry TX/SBUS2 slot behavior is claimed in this stage.
 */
#include "serial_uart_rx.h"
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
#include "serial_frame_rx.h"
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
    !defined(AM13E_BOARD_SERIAL_RX_BUSCLK_HZ)
#error "Board UART RX requires dedicated UC, RX pin/function, verified BUSCLK"
#endif
_Static_assert(AM13E_BOARD_SERIAL_RX_PINCM>0U &&
               AM13E_BOARD_SERIAL_RX_PINCM<107U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION>0U &&
               AM13E_BOARD_SERIAL_RX_PIN_FUNCTION<8U &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ>=UINT32_C(2000000) &&
               AM13E_BOARD_SERIAL_RX_BUSCLK_HZ<=UINT32_C(200000000),
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

#if AM13E_BOARD_SERIAL_RX_UC_INDEX==0
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM0)
#define RX_IRQ UC0_INT_IRQn
#define RX_HANDLER UC0_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==1
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM1)
#define RX_IRQ UC1_INT_IRQn
#define RX_HANDLER UC1_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==2
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM2)
#define RX_IRQ UC2_INT_IRQn
#define RX_HANDLER UC2_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==3
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM3)
#define RX_IRQ UC3_INT_IRQn
#define RX_HANDLER UC3_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==4
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM4)
#define RX_IRQ UC4_INT_IRQn
#define RX_HANDLER UC4_IRQHandler
#elif AM13E_BOARD_SERIAL_RX_UC_INDEX==5
#define RX_UC ((DL_UNICOMM_Inst_Regs *)UNICOMM5)
#define RX_IRQ UC5_INT_IRQn
#define RX_HANDLER UC5_IRQHandler
#else
#error "Select AM13E UNICOMM0..UNICOMM5, with no other owner"
#endif

#define RX_ERRORS (DL_UART_ERROR_OVERRUN | DL_UART_ERROR_FRAMING | \
                   DL_UART_ERROR_PARITY | DL_UART_ERROR_BREAK)
#define RX_IRQ_ERRORS (DL_UART_INTERRUPT_OVERRUN_ERROR | \
                       DL_UART_INTERRUPT_FRAMING_ERROR | \
                       DL_UART_INTERRUPT_PARITY_ERROR | \
                       DL_UART_INTERRUPT_NOISE_ERROR)
static AM13E_SerialFrameRx rx_state;
static unsigned initialized;

void RX_HANDLER(void);

bool am13e_app_serial_rx_init(unsigned mode)
{
    if(__get_PRIMASK()!=1U || initialized ||
       (mode!=3U && mode!=4U))return false;
    /* UCS are shared with I2C/SPI; board profile must assign a
     * DEDICATED instance that no other owner has initialized.
     */
    DL_UART_reset(RX_UC);
    DL_UART_enablePower(RX_UC);
    if(!DL_UART_isPowerEnabled(RX_UC))return false;
    DL_GPIO_initPeripheralInputFunctionFeatures(
        AM13E_BOARD_SERIAL_RX_PINCM,
        AM13E_BOARD_SERIAL_RX_PIN_FUNCTION,
        mode==4U ? DL_GPIO_INVERSION_ENABLE : DL_GPIO_INVERSION_DISABLE,
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
    DL_UART_Config config={
        .mode=DL_UART_MODE_NORMAL,
        .direction=DL_UART_DIRECTION_RX,
        .flowControl=DL_UART_FLOW_CONTROL_NONE,
        .parity=mode==4U ? DL_UART_PARITY_EVEN : DL_UART_PARITY_NONE,
        .wordLength=DL_UART_WORD_LENGTH_8_BITS,
        .stopBits=mode==4U ? DL_UART_STOP_BITS_TWO :
                               DL_UART_STOP_BITS_ONE
    };
    DL_UART_init(RX_UC,&config);
    DL_UART_configBaudRate(RX_UC,AM13E_BOARD_SERIAL_RX_BUSCLK_HZ,
                           mode==4U?100000U:115200U);
    DL_UART_enableFIFOs(RX_UC);
    DL_UART_setRXFIFOThreshold(RX_UC,DL_UART_RX_FIFO_LEVEL_NOT_EMPTY);
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
        uint8_t bytes[32];
        unsigned len=0U;
        if(am13e_serial_frame_byte(&rx_state,
                (uint8_t)(raw&UNICOMMUART_RXDATA_DATA_MASK),
                bytes,&len)){
            if(rx_state.mode==3U)
                (void)am13e_app_io_ibus_frame(bytes,len);
            else
                (void)am13e_app_io_sbus_frame(bytes,len);
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
