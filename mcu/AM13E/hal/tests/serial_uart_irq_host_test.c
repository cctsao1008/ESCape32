/* Production UNICOMM ISR -> native framing -> original io.c/util.c.
 * Only registers, NVIC and command side effects are mocked. No decoder copy. */
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include "../serial_uart_rx.c"
#include "../command_reply.c"

GPIO_Regs test_gpio[3];
TestSCB test_scb;
uint32_t test_primask=1U;
unsigned test_invert,test_pin_invert[96];
TestTimer test_timer;
TestSysctl test_sysctl;
unsigned test_timer_stops,test_dma_disables;
static unsigned timer_count,timer_cc,timer_enabled,timer_event;
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static TestUART registers;
TestUC test_uc={&registers};
static unsigned irq_enabled,irq_priority,timer_priority,in_uart,rx_present,rx_poll;
static DL_UART_Config uart_config;
static unsigned tx_slots,wire_bytes,feeds,commands,baud,busy,inject_eot,arms;
static uint8_t wire[1024];
static DL_UART_IIDX reason;
void test_halt(void){assert(0);}
void am13e_app_motor_fault_shutdown(void){assert(0);}
void am13e_app_motor_fault_reset(void){assert(0);}
void am13e_pb14_resume_rx(void){assert(0);}
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
uint32_t NVIC_GetEnableIRQ(int irq){assert(irq==0 || irq==1);return irq==0?irq_enabled:1U;}
void NVIC_DisableIRQ(int irq){assert(irq>=0 && irq<=3);if(irq==0)irq_enabled=0U;}
void NVIC_EnableIRQ(int irq)
{
    assert(irq>=0 && irq<=3);if(irq!=0)return;irq_enabled=1U;
    if(inject_eot && !in_uart && crsf_tx.awaiting_eot){
        inject_eot=0U;interrupt(DL_UART_IIDX_EOT_DONE);
    }
}
void NVIC_SetPriority(int irq,unsigned p){assert(irq>=0 && irq<=2);if(irq==0)irq_priority=p;else if(irq==1)timer_priority=p;else assert(p==0U);}
unsigned NVIC_GetPriority(int irq){assert(irq==0 || irq==1);return irq==0?irq_priority:timer_priority;}
uint32_t NVIC_GetVector(int irq){assert(irq==0);return (uint32_t)(uintptr_t)UC0_IRQHandler|1U;}
void DL_UART_reset(TestUC *u){assert(u==&test_uc);}
void DL_UART_enablePower(TestUC *u){assert(u==&test_uc);}
bool DL_UART_isPowerEnabled(TestUC *u){assert(u==&test_uc);return true;}
void DL_UART_setClockConfig(TestUC *u,const DL_UART_ClockConfig *c){(void)u;(void)c;}
void DL_UART_init(TestUC *u,const DL_UART_Config *c){(void)u;uart_config=*c;}
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
void NVIC_ClearPendingIRQ(int irq){assert(irq==1 || irq==2);}
unsigned DL_Timer_getTimerCount(TestTimer *t){assert(t==TIMG4);return timer_count;}
void DL_Timer_disableInterrupt(TestTimer *t,unsigned mask)
{ assert(t==TIMG4);if(mask==SBUS2_IRQ)timer_enabled=0U; }
void DL_Timer_enableInterrupt(TestTimer *t,unsigned mask)
{ assert(t==TIMG4 && mask==SBUS2_IRQ);timer_enabled=1U; }
void DL_Timer_clearInterruptStatus(TestTimer *t,unsigned mask)
{ assert(t==TIMG4);if(mask==SBUS2_IRQ)timer_event=0U; }
void DL_Timer_setCaptureCompareValue(TestTimer *t,unsigned v,unsigned cc)
{ assert(t==TIMG4 && cc==1 && v<=UINT16_MAX);timer_cc=v; }
void DL_Timer_setClockConfig(TestTimer *t,const DL_Timer_ClockConfig *c)
{ assert(t==TIMG4 && c->divideRatio==0U);assert(c->prescale==(serial_timer_owner?99U:0U)); }
void DL_Timer_initTimerMode(TestTimer *t,const DL_Timer_TimerConfig *c)
{ assert(t==TIMG4);if(serial_timer_owner)assert(c->period==UINT16_MAX && c->timerMode==DL_TIMER_TIMER_MODE_PERIODIC_UP); }
void DL_Timer_setCaptureCompareCtl(TestTimer *t,unsigned m,unsigned c,unsigned cc)
{ assert(t==TIMG4 && cc==1);(void)m;(void)c; }
void DL_Timer_setCaptCompUpdateMethod(TestTimer *t,unsigned m,unsigned cc)
{ assert(t==TIMG4 && cc==1 && m==DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE); }
void DL_Timer_startCounter(TestTimer *t){assert(t==TIMG4);}
unsigned DL_Timer_getEnabledInterruptStatus(TestTimer *t,unsigned mask)
{ assert(t==TIMG4 && mask==SBUS2_IRQ);return timer_enabled?timer_event:0U; }
static void timer_irq(unsigned late)
{
    assert(timer_enabled && !in_uart && timer_priority==irq_priority);
    timer_count=(uint16_t)(timer_cc+late);timer_event=SBUS2_IRQ;
    TIMG4_0_IRQHandler();
}
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
static void sbus2(void)
{
    test_primask=1U;cfg.input_mode=4;
    assert(!am13e_pb14_release_timer_for_sbus2()); /* Not initialized. */
    am13e_pb14_bidir_tx_init();
    cfg.input_mode=0;
    assert(!am13e_pb14_release_timer_for_sbus2()); /* Digital receiver owner. */
    cfg.input_mode=4;state=TX_STREAM;
    assert(!am13e_pb14_release_timer_for_sbus2()); /* Live DMA cannot be stolen. */
    state=TX_IDLE;test_primask=0;
    assert(!am13e_pb14_release_timer_for_sbus2()); /* IRQs must be masked. */
    assert(!test_dma_disables);
    select_mode(4U);telmode=0;telphid=1;cfg.telem_poles=14;
    assert(test_dma_disables==1U);
    const unsigned stopped=test_timer_stops;
    test_primask=1U;
    assert(!am13e_pb14_release_timer_for_sbus2()); /* No second transfer. */
    test_primask=0U;
    assert(!am13e_pb14_bidir_tx_start(0U,100U,100000000U));
    assert(test_timer_stops==stopped && test_dma_disables==1U);
    temp1=25;temp2=-10;curr=0x3456;volt=0x5678;csum=0x1234;erpm=18000;
    assert(serial_timer_owner && test_invert==1U && baud==100000U);
    assert(am13e_pb14_tx_timer_irq_priority()==1U && timer_priority==1U);
    uint8_t p[25]={0x0f};p[24]=4U;
    timer_count=65000U;
    frame(p,25U);
    assert(sbus2_active && sbus2_count==6U && !wire_bytes);
    const unsigned before=feeds;
    frame(p,25U);assert(feeds==before); /* RX muted in reply window. */
    const uint8_t ids[6]={0x43,0xc3,0x23,0xa3,0x63,0xe3};
    for(unsigned n=0U;n<6U;++n){
        assert(timer_cc==(uint16_t)(65000U+3200U+660U*n));
        if(n==2U)curr=0x2345; /* Original Core samples this slot's live value. */
        timer_irq(n==2U?40U:0U); /* Absolute next deadlines do not drift. */
        assert(wire_bytes==3U*(n+1U) && wire[3U*n]==ids[n]);
        interrupt(DL_UART_IIDX_EOT_DONE);
        assert(sbus2_active);
    }
    assert(wire[1]==125U && wire[2]==0x80U && wire[4]==90U &&
        wire[7]==(0x23U|0x40U) && wire[8]==0x45U &&
        wire[10]==0x56U && wire[11]==0x78U &&
        wire[13]==0x12U && wire[14]==0x34U && wire[16]==(uint8_t)(18000/42));
    assert(timer_cc==(uint16_t)(65000U+7160U));
    timer_irq(0U);assert(!sbus2_active && !timer_enabled);
    /* Other phase group mutes RX for7160us without sending another group's slots. */
    p[24]=0x14U;frame(p,25U);assert(sbus2_active);
    timer_irq(0U);assert(sbus2_active && wire_bytes==18U);
    timer_irq(0U);assert(!sbus2_active && wire_bytes==18U);
    /* Ordinary SBUS does not schedule any reply. */
    p[24]=0U;frame(p,25U);assert(!sbus2_active && !timer_enabled);
    /* Too-late slot: drop remaining output rather than compress the schedule. */
    p[24]=4U;frame(p,25U);timer_irq(300U);
    assert(wire_bytes==18U && timer_cc==(uint16_t)(sbus2_origin+7160U));
    timer_irq(0U);assert(!sbus2_active);
    /* Delayed TX completion cannot overwrite a queue or release RX early. */
    frame(p,25U);timer_irq(0U);assert(wire_bytes==21U);
    timer_irq(0U);assert(wire_bytes==21U); /* Outstanding EOT cancels later slots. */
    timer_irq(0U);assert(sbus2_active && sbus2_release_due);
    interrupt(DL_UART_IIDX_EOT_DONE);assert(!sbus2_active);
    /* Cancelled/duplicate timer delivery cannot transmit stale payload. */
    timer_event=SBUS2_IRQ;am13e_app_serial_sbus2_timer_irq();
    assert(wire_bytes==21U && serial_timer_owner);
    frame(p,25U);serial_rx_tick+=1049U; /* >one full16-bit timer turn. */
    timer_irq(0U);assert(!sbus2_active && wire_bytes==21U);
}
static void cli_from_sbus(void)
{
    /* Fresh simulated boot, not a live hardware ownership transfer. */
    serial_timer_owner=tx_initialized=sbus2_timer_owned=false;
    state=TX_IDLE;test_primask=1U;am13e_pb14_bidir_tx_init();
    select_mode(4U);
    assert(test_pin_invert[AM13E_BOARD_SERIAL_RX_PINCM]==1U &&
        test_pin_invert[AM13E_BOARD_SERIAL_TX_PINCM]==1U);
    cli_entry.requested=1U;test_primask=1U;
    assert(am13e_app_serial_cli_enter());
    assert(baud==38400U && uart_config.parity==DL_UART_PARITY_NONE &&
        uart_config.stopBits==DL_UART_STOP_BITS_ONE &&
        !test_pin_invert[AM13E_BOARD_SERIAL_RX_PINCM] &&
        !test_pin_invert[AM13E_BOARD_SERIAL_TX_PINCM]);
    assert(!sbus2_active && !timer_enabled);
}
int main(void)
{
    crsf();serial();cli();sbus2();cli_from_sbus();
    puts("PASS production UART/Core, TIMG4 static ownership and SBUS2 deadlines/mute/FIFO/EOT; CLI polarity/preemption");
    return 0;
}
