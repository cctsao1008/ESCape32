/* Real capture/reply HAL, SDK register layouts/DriverLib, decoder and
 * original io.c policy. Only autonomous timer/DMA/W1C and CPU primitives
 * are modeled in anonymous RAM; no duplicate receiver implementation. */
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "common.h"
#define savecfg fixture_unused_savecfg
#define resetcfg fixture_unused_resetcfg
#define playmusic fixture_unused_playmusic
#define resetcom fixture_unused_resetcom
#include "../../../../src/util.c"
#undef savecfg
#undef resetcfg
#undef playmusic
#undef resetcom
#include "../command_capture.c"
static unsigned reconfiguration_cycles;
static void fixture_timer_setup(GPTIMER_Regs *t,DL_Timer_TimerConfig *c)
{
    DL_Timer_initTimerMode(t,c); /* Actual SDK programming still executes. */
    const unsigned reload=SysTick->LOAD+1U;
    SysTick->VAL=(SysTick->VAL+reload-reconfiguration_cycles)%reload;
}
#define DL_Timer_initTimerMode fixture_timer_setup
#include "../command_reply.c"
#undef DL_Timer_initTimerMode
uint32_t test_primask=1U;TestSCB test_scb;
TestSysTick test_systick={.LOAD=12499U,.VAL=12000U};
static unsigned enabled[80],priority[80],feeds,arms,saves;
static uint32_t time_now;
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
static void timer_to(uint32_t t)
{
    while((time_now>>16)!=(t>>16)){
        time_now=(time_now&0xffff0000U)+0x10000U;
        *(volatile uint32_t *)(uintptr_t)&TIMG4->COUNTERREGS.CTR=0U;
        *(volatile uint32_t *)(uintptr_t)&TIMG4->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
        TIMG4_0_IRQHandler();
        *(volatile uint32_t *)(uintptr_t)&TIMG4->CPU_INT.MIS=0U;
    }
    time_now=t;
    *(volatile uint32_t *)(uintptr_t)&TIMG4->COUNTERREGS.CTR=t&0xffffU;
}
static void capture_edge(uint32_t t,int poll)
{
    timer_to(t);
    assert((TIMG4->COUNTERREGS.CCCTL_01[0]&GPTIMER_CCCTL_01_COC_MASK)==DL_TIMER_CC_MODE_CAPTURE);
    assert(!DL_Timer_isCaptureCompareInputFilterEnabled(TIMG4,DL_TIMER_CC_0_INDEX));
    assert(DL_Timer_getEnabledEvents(TIMG4,DL_TIMER_GEN_EVENT1,RX_EVENTS)==RX_EVENTS);
    assert(DL_DMA_isChannelEnabled(DMA0,2U));
    assert(DL_DMA_getSrcAddr(DMA0,2U)==(uint32_t)(uintptr_t)&TIMG4->COUNTERREGS.CC_01[0]);
    TIMG4->COUNTERREGS.CC_01[0]=t&0xffffU;
    unsigned n=DL_DMA_getTransferSize(DMA0,2U);assert(n>0 && n<=32U);
    *(volatile uint32_t *)(uintptr_t)&GPIO0->DIN31_0=
        (((32U-n)&1U)?inverted_rx:!inverted_rx)?COMMAND_PIN:0U;
    edges[32U-n]=TIMG4->COUNTERREGS.CC_01[0];
    DL_DMA_setTransferSize(DMA0,2U,n-1U); /* DMA completed-word accounting. */
    if(n==1U){
        DL_DMA_disableChannel(DMA0,2U);
        *(volatile uint32_t *)(uintptr_t)&DMA0->CPU_INT.MIS=DL_DMA_INTERRUPT_CHANNEL2;
        DMA0_IRQHandler();
    }else if(n==17U){
        *(volatile uint32_t *)(uintptr_t)&DMA0->CPU_INT.MIS=DL_DMA_FULL_CH_INTERRUPT_EARLY_CHANNEL2;
        DMA0_IRQHandler();
    }else if(poll)am13e_app_pb14_systick();
}
static uint16_t packet(unsigned v,int inv)
{
    unsigned x=v<<1U;return (uint16_t)((x<<4U)|((x^(x>>4U)^(x>>8U)^(inv?15U:0U))&15U));
}
static void frame(unsigned period,uint16_t x,int partial)
{
    uint32_t t=time_now+10000U;
    for(unsigned i=0;i<16U;++i){
        capture_edge(t,partial);
        capture_edge(t+((x&(1U<<(15U-i)))?period*3U/4U:period*3U/8U),partial);
        t+=period;
    }
}
int main(void)
{
    assert(mmap((void*)0x40000000,0x07000000,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)0x40000000);
    assert(!setjmp(fault));
    SYSCTL->SOCLOCK.MCLKCFG=DL_SYSCTL_MCLK_DIV_2_DIV_4;
    cfg.input_mode=0;cfg.throt_min=1000;cfg.throt_mid=1500;cfg.throt_max=2000;
    initio();assert(DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA26)==3U);
    const unsigned rates[]={333U,167U,83U};
    for(unsigned r=0;r<3U;++r){
        am13e_pb14_decoder_reset(&decoder,RX_HZ,0);am13e_pb14_resume_rx();time_now=0;
        const unsigned before=feeds;
        frame(rates[r],packet(512,0),r&1U);
        assert(feeds==before+1U && throt==465);
        frame(rates[r],packet(512,0)^1U,1);assert(feeds==before+1U);
        assert(DL_DMA_getTransferSize(DMA0,2U)==32U);
    }
    /* Truncated frame followed by a complete one crosses the SRAM block
     * boundary: DMA completion is not a protocol frame boundary. */
    am13e_pb14_decoder_reset(&decoder,RX_HZ,0);am13e_pb14_resume_rx();time_now=0;
    frame(167U,packet(512,0),1);
    unsigned truncated_before=feeds;
    for(unsigned i=0;i<4U;++i){uint32_t t=time_now+167U;capture_edge(t,1);capture_edge(t+60U,1);}
    frame(167U,packet(512,0),1);
    assert(feeds==truncated_before+1U);
    /* Source commands execute only after six telemetry-requested frames;
     * persistence is deferred, never executed from capture/DMA IRQ. */
    am13e_pb14_decoder_reset(&decoder,RX_HZ,0);am13e_pb14_resume_rx();time_now=0;
    frame(167U,packet(0,0),1);ertm=0;
    unsigned x=(12U<<1U)|1U;
    uint16_t save=(uint16_t)((x<<4U)|((x^(x>>4U)^(x>>8U))&15U));
    for(unsigned i=0;i<6U;++i)frame(167U,save,1);
    assert(saves==0U);
    am13e_app_io_service_pending_save();assert(saves==1U);
    const unsigned rejected_feeds=feeds;
    /* Zero width and a >2.5ms pulse must not reach the Core watchdog. */
    capture_edge(time_now+10000U,1);capture_edge(time_now,1);
    capture_edge(time_now+10000U,1);capture_edge(time_now+300000U,1);
    assert(feeds==rejected_feeds && invalid_capture_groups>=2U);
    am13e_pb14_resume_rx();time_now=0;
    /* DMA backlog: completed block but CC register has a newer edge. */
    const unsigned before=feeds,over=capture_overruns;
    DL_DMA_setTransferSize(DMA0,2U,0);edges[31]=100U;TIMG4->COUNTERREGS.CC_01[0]=101U;
    am13e_pa26_rx_dma_irq(); /* no status: stale vector is ignored. */
    drain();assert(capture_overruns==over+1U && feeds==before && capture_desynced);
    *(volatile uint32_t *)(uintptr_t)&GPIO0->DIN31_0=0U;
    timer_to(2000U);am13e_app_pb14_systick();
    assert(!capture_desynced && DL_DMA_getTransferSize(DMA0,2U)==32U);
    /* Wrong final edge polarity quarantines the prefix before CRC/Core,
     * then a quiet idle restores the next frame's edge pairing. */
    am13e_pb14_decoder_reset(&decoder,RX_HZ,0);am13e_pb14_resume_rx();time_now=0;
    capture_edge(1000U,0);
    *(volatile uint32_t *)(uintptr_t)&GPIO0->DIN31_0=0U;
    am13e_app_pb14_systick();assert(capture_desynced && feeds==before);
    timer_to(4000U);am13e_app_pb14_systick();assert(!capture_desynced);
    frame(167U,packet(512,0),1);assert(feeds==before+1U);
    /* Servo and Oneshot timestamps span multiple real 16-bit wraps. */
    for(unsigned mode=0;mode<2U;++mode){
        am13e_pb14_decoder_reset(&decoder,RX_HZ,0);am13e_pb14_resume_rx();time_now=0;
        unsigned width=mode?18750U:150000U,period=mode?30000U:2000000U;
        for(unsigned i=0;i<16U;++i){
            uint32_t t=time_now+period;
            capture_edge(t,1);capture_edge(t+width,1);
        }
        assert(decoder.rx_mode==(mode?AM13E_PB14_RX_ONESHOT:AM13E_PB14_RX_PWM));
        assert(decoder.good_pwm>0U);
    }
    /* Inverted native RX -> delay -> GPIO26 DMA TX -> native RX. */
    for(unsigned repeat=0;repeat<3U;++repeat){
    inverted_rx=1;am13e_pb14_decoder_reset(&decoder,RX_HZ,1);
    am13e_pb14_resume_rx();time_now=0;
    reconfiguration_cycles=repeat<2U?100U:0U;
    SysTick->VAL=repeat==1U?20U:12000U;
    const unsigned inverted_before=feeds;
    frame(rates[repeat],packet(512,1),1);assert(state==TX_DELAY && suspended);
    assert(feeds==inverted_before+1U);
    assert(TIMG4->COUNTERREGS.LOAD==2999U-reconfiguration_cycles/2U);
    reconfiguration_cycles=0U;
    assert(!DL_DMA_isChannelEnabled(DMA0,2U));
    *(volatile uint32_t *)(uintptr_t)&TIMG4->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
    TIMG4_0_IRQHandler();
    assert(state==TX_STREAM && DL_GPIO_getPeripheralFunctionBits(COMMAND_PINCM)==1U);
    assert(DL_DMA_getDestAddr(DMA0,2U)==(uint32_t)(uintptr_t)&GPIO0->DOUTTGL31_0);
    assert((GPIO0->DOE31_0&COMMAND_PIN)!=0U);
    *(volatile uint32_t *)(uintptr_t)&DMA0->CPU_INT.MIS=DL_DMA_INTERRUPT_CHANNEL2;
    DMA0_IRQHandler();assert(state==TX_IDLE && !suspended);
    assert((GPIO0->DOE31_0&COMMAND_PIN)==0U);
    assert(DL_GPIO_getPeripheralFunctionBits(COMMAND_PINCM)==3U);
    assert(DL_DMA_getSrcAddr(DMA0,2U)==(uint32_t)(uintptr_t)&TIMG4->COUNTERREGS.CC_01[0]);
    *(volatile uint32_t *)(uintptr_t)&DMA0->CPU_INT.MIS=0U;DMA0_IRQHandler();
    *(volatile uint32_t *)(uintptr_t)&TIMG4->CPU_INT.MIS=0U;TIMG4_0_IRQHandler();
    assert(!suspended && state==TX_IDLE);
    const unsigned invalid_before=feeds;
    frame(rates[repeat],packet(512,1)^1U,1);
    assert(feeds==invalid_before && state==TX_DELAY); /* Rel17 replies precede CRC. */
    *(volatile uint32_t *)(uintptr_t)&TIMG4->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
    TIMG4_0_IRQHandler();
    *(volatile uint32_t *)(uintptr_t)&DMA0->CPU_INT.MIS=DL_DMA_INTERRUPT_CHANNEL2;
    DMA0_IRQHandler();assert(state==TX_IDLE && !suspended && feeds==invalid_before);
    }
    /* Reconfiguration consumes the hardware deadline: reject, release
     * ownership and restore input rather than sending a late reply. */
    am13e_pb14_decoder_reset(&decoder,RX_HZ,1);am13e_pb14_resume_rx();time_now=0;
    reconfiguration_cycles=6000U;
    frame(167U,packet(512,1),1);assert(state==TX_IDLE && !suspended);
    reconfiguration_cycles=0U;
    /* No TX completion: original timeout restores capture without a stale reply. */
    am13e_pb14_decoder_reset(&decoder,RX_HZ,1);am13e_pb14_resume_rx();time_now=0;
    frame(167U,packet(512,1),1);assert(state==TX_DELAY);
    for(unsigned i=0;i<17U;++i)am13e_app_pb14_systick();
    assert(state==TX_IDLE && !suspended && (GPIO0->DOE31_0&COMMAND_PIN)==0U);
    puts("PASS production PA26 TIMG4 capture/DMA -> original io.c DShot300/600/1200 CRC/throttle; wraps/Servo/Oneshot; BiDShot ownership/pinmux/stale IRQ");
}
