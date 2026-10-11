/* Production UNICOMM ISR -> native framing -> original io.c/util.c.
 * Only registers, NVIC and command side effects are mocked. No decoder copy. */
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include "../serial_uart_rx.c"

GPIO_Regs test_gpio[3];
TestSCB test_scb;
uint32_t test_primask=1U;
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static TestUART registers;
TestUC test_uc={&registers};
static unsigned irq_enabled,irq_priority,in_uart,rx_present,rx_poll;
static unsigned tx_slots,wire_bytes,feeds,commands,baud,busy,inject_eot,arms;
static uint8_t wire[1024];
static DL_UART_IIDX reason;
void test_halt(void){assert(0);}
void am13e_app_io_watchdog_feed(void){++feeds;}
void am13e_app_io_watchdog_arm(void){++arms;}
void am13e_app_service_reset(void){assert(0);}
bool am13e_app_pb14_service_takeover(void){return true;}
int execcrsfcmd(const char *p,int n,char *reply)
{
    assert(in_uart==0U && n==3 && (uint8_t)p[0]==0x28U);
    ++commands;reply[0]=0x21;reply[1]=0x22;reply[2]=0x23;return 3;
}
int execcmd(char *line)
{
    assert(!in_uart && !strcmp(line,"status\n"));
    for(unsigned i=0;i<130U;++i)line[i]=(char)('A'+i%26U);
    return 130;
}
static void interrupt(DL_UART_IIDX event)
{
    assert(irq_enabled && !in_uart && !test_primask);
    reason=event;in_uart=1U;UC0_IRQHandler();in_uart=0U;
}
uint32_t NVIC_GetEnableIRQ(int irq){assert(irq==0);return irq_enabled;}
void NVIC_DisableIRQ(int irq){assert(irq==0);irq_enabled=0U;}
void NVIC_EnableIRQ(int irq)
{
    assert(irq==0);irq_enabled=1U;
    if(inject_eot && !in_uart && crsf_tx.awaiting_eot){
        inject_eot=0U;interrupt(DL_UART_IIDX_EOT_DONE);
    }
}
void NVIC_SetPriority(int irq,unsigned p){assert(irq==0);irq_priority=p;}
unsigned NVIC_GetPriority(int irq){assert(irq==0);return irq_priority;}
uint32_t NVIC_GetVector(int irq){assert(irq==0);return (uint32_t)(uintptr_t)UC0_IRQHandler|1U;}
void DL_UART_reset(TestUC *u){assert(u==&test_uc);}
void DL_UART_enablePower(TestUC *u){assert(u==&test_uc);}
bool DL_UART_isPowerEnabled(TestUC *u){assert(u==&test_uc);return true;}
void DL_UART_setClockConfig(TestUC *u,const DL_UART_ClockConfig *c){(void)u;(void)c;}
void DL_UART_init(TestUC *u,const DL_UART_Config *c){(void)u;(void)c;}
void DL_UART_configBaudRate(TestUC *u,unsigned clock,unsigned b)
{ (void)u;assert(clock==100000000U && !in_uart);baud=b; }
void DL_UART_enableFIFOs(TestUC *u){(void)u;}
void DL_UART_setRXFIFOThreshold(TestUC *u,unsigned t){(void)u;(void)t;}
void DL_UART_setTXFIFOThreshold(TestUC *u,unsigned t){(void)u;(void)t;}
void DL_UART_clearInterruptStatus(TestUC *u,unsigned mask){(void)u;(void)mask;}
void DL_UART_enableInterrupt(TestUC *u,unsigned mask)
{
    (void)u;
    /* An enabled UART may preempt PendSV at this exact publication point. */
    if(mask&DL_UART_INTERRUPT_EOT_DONE)assert(in_uart || !irq_enabled);
}
void DL_UART_disableInterrupt(TestUC *u,unsigned mask){(void)u;(void)mask;}
void DL_UART_enable(TestUC *u){(void)u;}
void DL_UART_disable(TestUC *u){(void)u;assert(!in_uart);}
bool DL_UART_isRXFIFOEmpty(TestUC *u){(void)u;return !rx_present;}
bool DL_UART_isTXFIFOFull(TestUC *u){(void)u;return !tx_slots;}
bool DL_UART_isBusy(TestUC *u){(void)u;return busy!=0U;}
DL_UART_IIDX DL_UART_getPendingInterrupt(TestUC *u)
{
    (void)u;
    DL_UART_IIDX r=reason;reason=DL_UART_IIDX_NO_INTERRUPT;
    if(rx_poll++){rx_present=0U;}
    return r;
}
void DL_UART_transmitData(TestUC *u,uint8_t b)
{ (void)u;assert(tx_slots && wire_bytes<sizeof wire);--tx_slots;wire[wire_bytes++]=b; }
static void byte(uint32_t b)
{
    registers.RXDATA=b;rx_present=1U;rx_poll=0U;interrupt(TEST_UART_RX);
}
static void frame(const uint8_t *p,unsigned n){for(unsigned i=0;i<n;++i)byte(p[i]);}
static void select_mode(unsigned m)
{
    initialized=0U;irq_enabled=0U;rx_present=0U;rx_poll=0U;test_primask=1U;
    memset(&crsf_tx,0,sizeof crsf_tx);memset(&crsf_cmd,0,sizeof crsf_cmd);
    wire_bytes=0U;tx_slots=200U;crsf_tx_available=false;
    cfg.input_mode=(char)m;cfg.input_ch1=1;cfg.input_ch2=2;
    cfg.throt_min=1000;cfg.throt_mid=1500;cfg.throt_max=2000;
    cfg.duty_drag=80;cfg.throt_mode=0;throt=0;ertm=0;
    assert(am13e_app_serial_rx_init(m) && am13e_app_serial_rx_irq_ready());
    test_primask=0U;
}
static void crsf(void)
{
    select_mode(5U);telmode=4;
    uint8_t p[]={0xc8,4,0x28,0,0x91,0};p[5]=crc8dvbs2((char*)p+2,3);
    const unsigned before=feeds;
    p[5]^=1U;frame(p,sizeof p);am13e_app_serial_crsf_service_pending();
    assert(feeds==before && !commands && !wire_bytes && arms==1U);
    p[5]^=1U;frame(p,sizeof p);
    assert(crsf_cmd.busy && !commands && !wire_bytes);
    tx_slots=2U;am13e_app_serial_crsf_service_pending();
    assert(commands==1U && wire_bytes==2U && crsf_tx.count==6U);
    frame(p,sizeof p);am13e_app_serial_crsf_service_pending();
    assert(commands==1U); /* No command execution while ACK is in flight. */
    tx_slots=2U;interrupt(DL_UART_IIDX_TX);assert(wire_bytes==4U);
    tx_slots=2U;interrupt(DL_UART_IIDX_TX);
    assert(wire_bytes==6U && crsf_tx.count==6U && crsf_tx.awaiting_eot);
    assert(wire[0]==0xc8 && wire[1]==4 && wire[2]==0x21 &&
           crc8dvbs2((char*)wire+2,4)==0);
    interrupt(DL_UART_IIDX_EOT_DONE);assert(!crsf_tx.count);
}
static void serial(void)
{
    select_mode(2U);
    uint8_t p[]={5,0,0xd0,7,0};p[4]=crc8((char*)p,4);
    frame(p,sizeof p);assert(throt==2000);
    throt=0;p[4]^=1U;const unsigned before=feeds;
    frame(p,sizeof p);assert(!throt && feeds==before);
    p[4]^=1U;byte(p[0]);byte(p[1]|UNICOMMUART_RXDATA_FRMERR_SET);
    frame(p,sizeof p);assert(throt==2000); /* Error abort, next frame recovers. */
    p[0]=3;p[1]=0x80;p[2]=crc8((char*)p,2);
    tx_slots=200U;frame(p,3U);assert(wire_bytes==12U && crsf_tx.count==12U);
    assert(crc8((char*)wire,12)==0);interrupt(DL_UART_IIDX_EOT_DONE);
}
static void cli(void)
{
    select_mode(0U);cli_entry.requested=1U;throt=0;test_primask=1U;
    assert(am13e_app_serial_cli_enter() && baud==38400U);
    test_primask=0U;
    frame((const uint8_t*)"status\n",7U);
    inject_eot=1U;am13e_app_serial_cli_service_pending();
    /* An EOT preemption queues chunk 2 using the published offset. */
    assert(wire_bytes==128U && cli_reply_offset==128U);
    interrupt(DL_UART_IIDX_EOT_DONE);assert(wire_bytes==130U);
    interrupt(DL_UART_IIDX_EOT_DONE);assert(!crsf_tx.count && !cli_reply_length);
    for(unsigned i=0;i<130U;++i)assert(wire[i]==(uint8_t)('A'+i%26U));
    assert(arms==2U); /* Text CLI is not receiver selection. */
}
int main(void)
{
    crsf();serial();cli();
    puts("PASS production UART IRQ/Core CRC, throttle, deferred command, FIFO/EOT and CLI preemption");
    return 0;
}
