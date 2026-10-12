/* Actual Boot transport + official GPIO DriverLib. CPU SysTick and the
 * external peer waveform are modeled; no physical Flash or GPIO access. */
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <soc.h>
#include <dl_gpio.h>
uint32_t test_primask;TestSCB test_scb;
void test_halt(void){assert(0);}
static uint32_t ticks;
static unsigned receiving,bad_stop,tx_count;
static uint8_t rx_byte;
static uint32_t tx_time[1000],tx_low[1000];
static uint32_t peer(GPIO_Regs *g,uint32_t mask)
{
    assert(g==GPIO0 && mask==(1U<<26));
    if(!receiving || ticks<1000U)return mask;
    unsigned bit=(ticks-1000U)/833U;
    if(bit==0U)return 0U;
    if(bit<=8U)return (rx_byte&(1U<<(bit-1U)))?mask:0U;
    return bad_stop?0U:mask;
}
#define DL_GPIO_readPins peer
#include "../mcu/AM13E/hal/io.c"
void DL_SYSTICK_init(uint32_t n){assert(n==0x1000000U);ticks=0;}
void DL_SYSTICK_enable(void){}
uint32_t DL_SYSTICK_getValue(void)
{
    ticks+=16U;
    if(!receiving && tx_count<1000U){
        tx_time[tx_count]=ticks;tx_low[tx_count++]=(GPIO0->DOE31_0&(1U<<26))!=0U;
    }
    return (0xffffffU-ticks)&0xffffffU;
}
int main(void)
{
    assert(mmap((void*)0x40000000,0x07000000,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)0x40000000);
    initio();assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==1U);
    assert(!(GPIO0->DOE31_0&(1U<<26)) && !(GPIO0->DOUT31_0&(1U<<26)));
    assert(boot_am13e_io_id()==4U);
    receiving=1;rx_byte=0xa5;char data=0;
    assert(recvbuf(&data,1) && (uint8_t)data==0xa5U);
    ticks=0;bad_stop=1;assert(!recvbuf(&data,1));
    receiving=0;bad_stop=0;ticks=tx_count=0;
    data=(char)0xa5;sendbuf(&data,1);
    for(unsigned bit=0;bit<10U;++bit){
        unsigned i=0;while(i+1U<tx_count && tx_time[i+1U]<417U+bit*833U)++i;
        unsigned expected=bit==0U?1U:bit==9U?0U:!((0xa5U>>(bit-1U))&1U);
        assert(tx_low[i]==expected);
    }
    assert(!(GPIO0->DOE31_0&(1U<<26)));
    /* Idle peer timeout and bad arguments return failure. */
    receiving=0;ticks=0;assert(!recvbuf(&data,1));
    assert(!recvbuf(NULL,1) && !recvbuf(&data,-1));
    puts("PASS actual Boot PA26 GPIO26 release/drive-low, 38400 8N1 RX/TX, stop rejection and timeout; interface ID unchanged");
}
