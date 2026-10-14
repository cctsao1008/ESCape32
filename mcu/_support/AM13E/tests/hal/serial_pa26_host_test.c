/* Production single-pad UART HAL and official SDK registers; only IIDX
 * acknowledge, FIFO status and CPU primitives are hardware fixtures. */
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include "common.h"
#define savecfg fixture_unused_savecfg
#define resetcfg fixture_unused_resetcfg
#define playmusic fixture_unused_playmusic
#define resetcom fixture_unused_resetcom
#include "../../../../../src/util.c"
#undef savecfg
#undef resetcfg
#undef playmusic
#undef resetcom
#include "../../../../AM13E/hal/serial_uart_rx.c"
#include "motor_power_stage.h"
unsigned fixture_uart_rx_pending;
uint32_t test_primask=1U;TestSCB test_scb;
static unsigned enabled[80],priority[80],feeds,arms,saves;

static jmp_buf fault;
char _cfg[4096];
void test_halt(void){longjmp(fault,1);}
void am13e_app_motor_fault_shutdown(void){}
void am13e_app_motor_fault_reset(void){longjmp(fault,1);}
void NVIC_SetPriority(IRQn_Type i,uint32_t p){priority[i]=p;}
void NVIC_EnableIRQ(IRQn_Type i){enabled[i]=1;}
void NVIC_DisableIRQ(IRQn_Type i){enabled[i]=0;}
void NVIC_ClearPendingIRQ(IRQn_Type i){(void)i;}
uint32_t NVIC_GetEnableIRQ(IRQn_Type i){return enabled[i];}
uint32_t NVIC_GetPriority(IRQn_Type i){return priority[i];}
uint32_t NVIC_GetVector(IRQn_Type i){(void)i;return 0;}
void am13e_app_io_watchdog_prepare(void){}
void am13e_app_io_watchdog_arm(void){++arms;}
void am13e_app_io_watchdog_feed(void){++feeds;}
void am13e_app_io_watchdog_status(uint32_t *a,uint32_t *f){*a=arms;*f=feeds;}
int savecfg(void){++saves;return 1;}
int resetcfg(void){return 1;}
int playmusic(const char *s,int v){(void)s;(void)v;return 1;}
void resetcom(void){}
void am13e_app_service_reset(void){longjmp(fault,1);}

void DL_Common_delayCycles(uint32_t n){(void)n;}
bool am13e_app_pb14_service_takeover(void){return true;}
bool am13e_pb14_release_timer_for_sbus2(void){return true;}
static void status(uint32_t value)
{*(volatile uint32_t *)(uintptr_t)&RX_UC->uart->STAT=value;}
static void rx_byte_native(uint32_t value)
{
    status(0U);fixture_uart_rx_pending=1U;
    *(volatile uint32_t *)(uintptr_t)&RX_UC->uart->RXDATA=value;
    *(volatile uint32_t *)(uintptr_t)&RX_UC->uart->CPU_INT.IIDX=DL_UART_IIDX_RX;
    RX_HANDLER();
}
int main(void)
{
    assert(mmap((void*)0x40000000,0x07000000,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)0x40000000);
    assert(!setjmp(fault));
    am13e_power_stage_init();am13e_power_stage_force_off();
    assert(!(GPIO1->DOE31_0&AM13E_GATE_MASK) && DL_GPIO_isInputEnabled(AM13E_GATE_PINCM));
#ifdef AM13E_BOARD_DEBUG_UART_BAUD
    am13e_app_debug_uart_init();
    assert(DL_GPIO_getPeripheralFunctionBits(AM13E_DEBUG_TX_PINCM)==9U &&
           DL_GPIO_getPeripheralFunctionBits(AM13E_DEBUG_RX_PINCM)==5U);
    *(volatile uint32_t *)(uintptr_t)&UC1_INST_PTR->uart->STAT=UNICOMMUART_STAT_RXFE_SET;
    uint8_t debug_byte=0U;
    assert(!am13e_app_debug_uart_try_read(&debug_byte));
    assert(am13e_app_debug_uart_try_write(0x5aU) && UC1_INST_PTR->uart->TXDATA==0x5aU);
    *(volatile uint32_t *)(uintptr_t)&UC1_INST_PTR->uart->STAT=0U;
    *(volatile uint32_t *)(uintptr_t)&UC1_INST_PTR->uart->RXDATA=0x23U;
    assert(am13e_app_debug_uart_try_read(&debug_byte) && debug_byte==0x23U);
#endif
    cfg.input_mode=2;cfg.throt_min=1000;cfg.throt_mid=1500;cfg.throt_max=2000;
    assert(!am13e_app_serial_rx_init(5U)); /* Full-duplex TX rejected. */
    assert(am13e_app_serial_rx_init(2U));
    const unsigned divisor=(800000000U/SERIAL_BR+1U)/2U;
    assert(DL_UART_getIntegerBaudRateDivisor(RX_UC)==divisor/64U &&
           DL_UART_getFractionalBaudRateDivisor(RX_UC)==divisor%64U);
    assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==8U);
    uint8_t input[]={5,0,0xd0,7,0};input[4]=crc8((char*)input,4);
    for(unsigned i=0;i<5U;++i)rx_byte_native(input[i]);
    assert(throt==2000 && feeds==1U);
    input[4]^=1U;
    for(unsigned i=0;i<5U;++i)rx_byte_native(input[i]);
    assert(feeds==1U);
    status(UNICOMMUART_STAT_RXFE_SET);
    const uint8_t reply[]={0x12,0x34,0x56};
    for(unsigned n=0;n<3U;++n){
        assert(crsf_tx_start(reply,sizeof reply));
        assert(wire_tx_active && DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==9U);
        assert(crsf_tx.awaiting_eot && RX_UC->uart->TXDATA==0x56U);
        /* FIFO empty is not EOT; RX stays disconnected until final bit. */
        RX_HANDLER();assert(wire_tx_active);
        *(volatile uint32_t *)(uintptr_t)&RX_UC->uart->CPU_INT.IIDX=DL_UART_IIDX_EOT_DONE;
        RX_HANDLER();
        assert(!wire_tx_active && crsf_tx.count==0U);
        assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==8U);
    }
    status(UNICOMMUART_STAT_RXFE_SET|UNICOMMUART_STAT_BUSY_SET);
    assert(!crsf_tx_start(reply,sizeof reply));
    assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==8U);
    status(0U);assert(!crsf_tx_start(reply,sizeof reply)); /* RX FIFO collision. */
    status(UNICOMMUART_STAT_RXFE_SET);assert(crsf_tx_start(reply,sizeof reply));
    status(UNICOMMUART_STAT_RXFE_SET|UNICOMMUART_STAT_RXCLR_SET|UNICOMMUART_STAT_TXCLR_SET);
    for(unsigned n=0;n<512U;++n)am13e_app_serial_rx_systick();
    assert(wire_tx_active && wire_abort_pending); /* SysTick cannot mutate a UART ISR. */
    am13e_app_serial_transport_service_pending();
    assert(!wire_tx_active && crsf_tx.count==0U && crsf_tx.dropped>0U);
    assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==8U);
    /* Actual configured SBUS2 initialization selects even parity/2 stop
     * bits and the TIMG4 deadline owner, with native inversion both ways. */
    initialized=0;crsf_tx_available=false;cfg.input_mode=4;
    SYSCTL->SOCLOCK.MCLKCFG=DL_SYSCTL_MCLK_DIV_2_DIV_4;
    assert(am13e_app_serial_rx_init(4U) && sbus2_timer_owned);
    assert((IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA26]&IOMUX_PINCM_INV_MASK)!=0U);
    status(UNICOMMUART_STAT_RXFE_SET);assert(crsf_tx_start(reply,sizeof reply));
    assert((IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA26]&IOMUX_PINCM_INV_MASK)!=0U);
    *(volatile uint32_t *)(uintptr_t)&RX_UC->uart->CPU_INT.IIDX=DL_UART_IIDX_EOT_DONE;
    RX_HANDLER();assert(!wire_tx_active && DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==8U);
    puts("PASS production PA26 UC0 RX/TX pinmux, FIFO versus EOT, repeated turnaround, busy/collision rejection, timeout flush/recovery; full-duplex rejection");
}
