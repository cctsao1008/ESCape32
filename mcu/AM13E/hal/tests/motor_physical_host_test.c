#include "motor_audio_hw.h"
#include "util_backend.h"
/* Original Core + native HAL + actual SDK registers/DriverLib. Only core
 * primitives, GPIO aliases and autonomous register loads are emulated. */
#include <sys/mman.h>
#define sync host_unused_posix_sync
#include <sys/wait.h>
#include <unistd.h>
#undef sync
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../../../../src/main.c"
#include "motor_safety.h"
#include "motor_output_runtime.h"
#include "board_motor_output_provider.h"
#include "motor_bemf.h"
#include "motor_event_timer.h"
#include "motor_hall.h"
#ifdef HALL_MAP
#include <dl_eqep.h>
#endif
#include <dl_mcpwm.h>
#include <dl_sysctl.h>
#include <dl_gpio.h>
#include <dl_ecap.h>
#include <dl_timer.h>
uint32_t test_primask=1U;
TestSCB test_scb;
static unsigned enabled[80],priority[80],watchdog_arms;
static jmp_buf fault_return;
void test_halt(void){longjmp(fault_return,1);}
void __error__(const char *file,uint32_t line){fprintf(stderr,"SDK assertion %s:%u\n",file,line);abort();}
void NVIC_SetPriority(IRQn_Type i,uint32_t p){assert(i>=0);priority[i]=p;}
void NVIC_ClearPendingIRQ(IRQn_Type i){assert(i>=0);}
void NVIC_EnableIRQ(IRQn_Type i){assert(i>=0);enabled[i]=1U;}
void NVIC_DisableIRQ(IRQn_Type i){assert(i>=0);enabled[i]=0U;}
uint32_t NVIC_GetEnableIRQ(IRQn_Type i){return enabled[i];}
uint32_t NVIC_GetPriority(IRQn_Type i){return priority[i];}
uint32_t NVIC_GetVector(IRQn_Type i){
#ifdef HALL_MAP
    if(i==GPIO0_INT_IRQn)return (uint32_t)(uintptr_t)GPIO0_IRQHandler|1U;
    if(i==EQEP0_INT_IRQn)return (uint32_t)(uintptr_t)EQEP0_IRQHandler|1U;
#endif
    (void)i;return 0U;
}
int am13e_app_io_watchdog_fault_arm(void){++watchdog_arms;return 1;}
void test_global_load(MCPWM_Regs *p)
{
    if(!(p->GLDOSHTSTS&MCPWM_GLDOSHTSTS_OSHTLATCH_MASK))return;
    assert(p->DBREDS!=0U && p->DBFEDS!=0U); /* TRM global ZERO restriction. */
    p->DBRED=p->DBREDS;p->DBFED=p->DBFEDS;
    p->CMPC=p->CMPCS;
    p->TBPRD=p->TBPRDS;
    p->PWM1_CMPA=p->PWM1_CMPAS;p->PWM1_CMPB=p->PWM1_CMPBS;
    p->PWM2_CMPA=p->PWM2_CMPAS;p->PWM2_CMPB=p->PWM2_CMPBS;
    p->PWM3_CMPA=p->PWM3_CMPAS;p->PWM3_CMPB=p->PWM3_CMPBS;
    p->PWM1_AQCTLA=p->PWM1_AQCTLAS;p->PWM1_AQCTLB=p->PWM1_AQCTLBS;
    p->PWM2_AQCTLA=p->PWM2_AQCTLAS;p->PWM2_AQCTLB=p->PWM2_AQCTLBS;
    p->PWM3_AQCTLA=p->PWM3_AQCTLAS;p->PWM3_AQCTLB=p->PWM3_AQCTLBS;
    *(volatile uint32_t *)(uintptr_t)&p->GLDOSHTSTS=0U;
    p->GLDOSHTCTL=0U;
}

static unsigned inject_shadow_zero,shadow_boundaries,inject_trip,inject_ack_trip;
void test_irq_ack_boundary(MCPWM_Regs *p)
{
    if(inject_ack_trip){
        p->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ1;
        p->INTFLAG|=DL_MCPWM_INT_TZ_OST;inject_ack_trip=0U;
    }
}
static void active_image(MCPWM_Regs *p,uint32_t image[16])
{
    const uint32_t values[16]={p->TBPRD,p->PWM1_CMPA,p->PWM1_CMPB,p->PWM2_CMPA,p->PWM2_CMPB,
        p->PWM3_CMPA,p->PWM3_CMPB,p->PWM1_AQCTLA,p->PWM1_AQCTLB,p->PWM2_AQCTLA,
        p->PWM2_AQCTLB,p->PWM3_AQCTLA,p->PWM3_AQCTLB,p->CMPC,p->DBRED,p->DBFED};
    memcpy(image,values,sizeof values);
}
void test_shadow_written(MCPWM_Regs *p)
{
    if(inject_trip){
        MCPWM0->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ1;
        MCPWM0->INTFLAG|=DL_MCPWM_INT_TZ_OST;inject_trip=0U;
    }
    if(!inject_shadow_zero)return;
    uint32_t before[16],after[16];active_image(p,before);
    test_global_load(p); /* ZERO during every individual register write. */
    active_image(p,after);assert(memcmp(before,after,sizeof before)==0);++shadow_boundaries;
}
/* TI AQ/RED register behavior observer, not a commutation controller.
 * Core chooses phases; the observer checks the resulting logical gate
 * levels at each TBCLK. This is model evidence, not a pin measurement. */
static unsigned aq_state[6],gate_state[6];
static uint64_t virtual_tick,red_deadline[3],last_off[6];
#ifdef PWM_ENABLE
static uint64_t fed_deadline[3];
#endif
static unsigned off_seen[6];
void test_aq_forced(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o)
{
    assert(p==MCPWM0 && (unsigned)o<6U);
    const uint32_t sw[3]={p->PWM1_AQSFRC,p->PWM2_AQSFRC,p->PWM3_AQSFRC};
    assert(((sw[(unsigned)o/2U]>>((unsigned)o%2U?MCPWM_PWM1_AQSFRC_PWMB_OFS:MCPWM_PWM1_AQSFRC_PWMA_OFS))&7U)==DL_MCPWM_AQ_SW_ONE_SHOT_LOW);
    aq_state[(unsigned)o]=gate_state[(unsigned)o]=0U;
    last_off[(unsigned)o]=virtual_tick;off_seen[(unsigned)o]=1U;
}
static void aq_event(unsigned i,unsigned action)
{
    if(action==1U)aq_state[i]=0U;
    else if(action==2U)aq_state[i]=1U;
    else if(action==3U)aq_state[i]^=1U;
}
static void carrier(void)
{
    uint32_t aq[6]={MCPWM0->PWM1_AQCTLA,MCPWM0->PWM1_AQCTLB,
        MCPWM0->PWM2_AQCTLA,MCPWM0->PWM2_AQCTLB,MCPWM0->PWM3_AQCTLA,MCPWM0->PWM3_AQCTLB};
    uint32_t cc[6]={MCPWM0->PWM1_CMPA,MCPWM0->PWM1_CMPB,
        MCPWM0->PWM2_CMPA,MCPWM0->PWM2_CMPB,MCPWM0->PWM3_CMPA,MCPWM0->PWM3_CMPB};
    const unsigned red=MCPWM0->DBRED;
#ifdef PWM_ENABLE
    assert(MCPWM0->DBFED==red && (MCPWM0->DBCTL&MCPWM_DBCTL_OUT_MODE_MASK)==3U);
#else
    assert(MCPWM0->DBFED==1U && (MCPWM0->DBCTL&MCPWM_DBCTL_OUT_MODE_MASK)==(1U<<DL_MCPWM_DB_RED));
#endif
    for(unsigned tick=0;tick<=MCPWM0->TBPRD;++tick,++virtual_tick){
        MCPWM0->TBCTR=tick;
#ifdef SW_BLANKING
        if(tick==AM13E_BEMF_BLANK_TICKS && (MCPWM0->INTEN&DL_MCPWM_INT_ET_2)){
            MCPWM0->INTFLAG=DL_MCPWM_INT_ET_2;MCPWM0_IRQHandler();
        }
#endif
        for(unsigned phase=0;phase<3U;++phase){
            const unsigned a=phase*2U,b=a+1U,old_a=aq_state[a];
            const unsigned previous[2]={gate_state[a],gate_state[b]};
            if(tick==0U){aq_event(a,aq[a]&3U);aq_event(b,aq[b]&3U);}
            if(tick==cc[a]){aq_event(a,(aq[a]>>4U)&3U);aq_event(b,(aq[b]>>4U)&3U);}
            if(tick==cc[b]){aq_event(a,(aq[a]>>8U)&3U);aq_event(b,(aq[b]>>8U)&3U);}
            if(!aq_state[a]){gate_state[a]=0U;red_deadline[phase]=UINT64_MAX;}
            else if(!old_a)red_deadline[phase]=virtual_tick+red;
            if(aq_state[a] && virtual_tick==red_deadline[phase])gate_state[a]=1U;
#ifdef PWM_ENABLE
            /* B is active-low bridge enable, not a low-side gate. FED
             * delays its activation; a rising input cancels a short pulse. */
            if(aq_state[b]){gate_state[b]=1U;fed_deadline[phase]=UINT64_MAX;}
            else if(previous[1]){
                if(fed_deadline[phase]==UINT64_MAX)fed_deadline[phase]=virtual_tick+MCPWM0->DBFED;
                if(virtual_tick==fed_deadline[phase])gate_state[b]=0U;
            }
#else
            gate_state[b]=aq_state[b];
            assert(!(gate_state[a] && gate_state[b]));
            for(unsigned leg=0;leg<2U;++leg){
                const unsigned i=a+leg;
                if(previous[leg] && !gate_state[i]){last_off[i]=virtual_tick;off_seen[i]=1U;}
            }
            for(unsigned leg=0;leg<2U;++leg){
                const unsigned i=a+leg,other=a+(1U-leg);
                if(!previous[leg] && gate_state[i] && off_seen[other])
                    assert(virtual_tick-last_off[other]>=red);
            }
#endif
        }
    }
}
static void expect_sixstep(void)
{
    /* Rel17 documented C|B, A|B, A|C, B|C, B|A, C|A sequence. */
    static const unsigned positive[6]={2,0,0,1,1,2};
    static const unsigned negative[6]={1,1,2,2,0,0};
    const uint32_t aq[6]={MCPWM0->PWM1_AQCTLA,MCPWM0->PWM1_AQCTLB,
        MCPWM0->PWM2_AQCTLA,MCPWM0->PWM2_AQCTLB,MCPWM0->PWM3_AQCTLA,MCPWM0->PWM3_AQCTLB};
    assert(step>=1 && step<=6);
    for(unsigned phase=0;phase<3U;++phase){
#ifdef PWM_ENABLE
        if(phase==positive[step-1]){
            assert(aq[phase*2U]==(DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA));
            assert(aq[phase*2U+1U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
        }else{
            assert(aq[phase*2U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
            assert(aq[phase*2U+1U]==(phase==negative[step-1] ?
                DL_MCPWM_AQ_OUTPUT_LOW_ZERO:DL_MCPWM_AQ_OUTPUT_HIGH_ZERO));
        }
#else
        if(phase==positive[step-1]){
            assert(aq[phase*2U]==(DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA));
        }else if(phase==negative[step-1]){
            assert(aq[phase*2U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
            assert(aq[phase*2U+1U]==DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
        }else if(phase!=positive[step-1]){
            assert(aq[phase*2U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
            assert(aq[phase*2U+1U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
        }
#endif
    }
}
static void capture(unsigned ticks)
{
    ECAP1->CAP1=ticks;ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;
    ECAP1_IRQHandler();
}
static void zero(void)
{
    test_global_load(MCPWM0);
    uint32_t events=DL_MCPWM_INT_ET_1;
    MCPWM0->TBCTR=0U;
    if(MCPWM0->INTEN&events){
        uint32_t mask=test_primask;test_primask=0U;
        MCPWM0->INTFLAG=MCPWM0->INTEN&events;MCPWM0_IRQHandler();test_primask=mask;
    }
}
static int gate_on(void)
{ return ((GPIO1->DOUT31_0&DL_GPIO_PIN(13U))!=0U)==(AM13E_MOTOR_GATE_ACTIVE_LEVEL!=0); }
static void start(void)
{
    am13e_app_motor_commutation_commit();
    MCPWM0->ETCNT|=1U<<MCPWM_ETCNT_ET1_CNT_OFS; /* queued pre-stop ZERO */
    am13e_app_motor_commutation_enable(1);
    assert(DL_MCPWM_getEventTriggerEventCount(MCPWM0,DL_MCPWM_ET_1)==0U);
    assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));
    assert(!gate_on());
    assert(!MCPWM0->PWM1_AQSFRC && !MCPWM0->PWM2_AQSFRC && !MCPWM0->PWM3_AQSFRC);
    zero();assert(!gate_on());
    zero();assert(gate_on());
}
#ifdef HALL_MAP
static void hall_input(unsigned code)
{
    *(volatile uint32_t *)(uintptr_t)&GPIO0->DIN31_0=
        ((code&1U)?DL_GPIO_PIN(AM13E_HALL_PIN1):0U)|
        ((code&2U)?DL_GPIO_PIN(AM13E_HALL_PIN2):0U)|
        ((code&4U)?DL_GPIO_PIN(AM13E_HALL_PIN3):0U);
}
static void hall_edge(unsigned code,unsigned ticks)
{
    hall_input(code);EQEP0->QUTMR=ticks;
    *(volatile uint32_t *)(uintptr_t)&GPIO0->CPU_INT.MIS=DL_GPIO_PIN(AM13E_HALL_PIN1);
    GPIO0_IRQHandler();
    assert(EQEP0->QUTMR==0U && GPIO0->CPU_INT.MIS==0U);
}
static void hall_runtime(void)
{
    am13e_app_motor_hall_gpio_init();
    for(unsigned code=0;code<8U;++code){hall_input(code);assert(getcode()==(int)code);}
    hall_input(7);am13e_app_motor_hall_init(0);
    assert(am13e_app_motor_hall_ready() && !enabled[GPIO0_INT_IRQn]);
    hall_input(1);am13e_app_motor_hall_init(1);
    assert(am13e_app_motor_hall_ready() && EQEP0->QUPRD==6553600U);
    cfg.damp=1;cfg.sine_power=8;ertm=6000;ival=10000;sync=6;hall=10000;
    am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);
    step=0;reverse=0;nextstep();start();carrier();
    static const unsigned expected[6][2]={{2,4},{4,6},{3,5},{6,2},{1,3},{5,1}};
    for(unsigned direction=0;direction<2U;++direction){
        reverse=direction;
        for(unsigned code=1;code<=6U;++code){
            hall=10000;sync=5;test_primask=0U;
            hall_edge(code,1000000U); /* 5 ms -> original 10000 half-us counts. */
            assert(test_primask==0U && step==(int)expected[code-1U][direction]);
            assert(ival==10000 && hall==10000 && sync==6);
            assert(!(TIMG12->COUNTERREGS.CTRCTL&GPTIMER_CTRCTL_EN_MASK));
            zero();expect_sixstep();carrier();
        }
    }
    test_primask=1U;
    /* Fast Hall edge follows original hybrid threshold: BEMF owns COM. */
    hall=4000;sync=6;const int old=step;
    hall_edge(3,100000U);assert(hall==3250 && step==old);
    /* Actual ECAP/BEMF policy conversion and TIMG12 delay in Hall builds. */
    ival=4000;cfg.timing=16;sync=0;
    am13e_app_motor_bemf_interval_select(6000);
    capture(300000U); /* Real ECAP1 ISR: 200 MHz capture of 1.5 ms. */
    assert(ival==3750 && sync==1);
    assert(TIMG12->COUNTERREGS.LOAD==46900U-1U);
    /* Hardware unit-timeout and coalesced GPIO/timeout keep UIF priority. */
    EQEP0->QFLG=DL_EQEP_INT_UNIT_TIME_OUT|DL_EQEP_INT_GLOBAL;
    EQEP0_IRQHandler();assert(hall==0x10000 && sync==0 && ival==10000);
    hall=10000;sync=6;EQEP0->QFLG=DL_EQEP_INT_UNIT_TIME_OUT;
    hall_edge(1,20U);assert(hall==0x10000 && sync==0 && step==old);
    am13e_app_motor_commutation_enable(0);sine=1000;step=0;prep=0;
    nextstep();assert(TIMG12->COUNTERREGS.LOAD==50000U-1U);
    const int sine_step=step;hall_edge(2,1000000U);assert(step==sine_step);
    am13e_app_motor_commutation_enable(0);sine=0;step=0;
    hall_edge(4,1000000U);assert(step==0 && !gate_on());
    /* Invalid Hall code is rejected by original getcode()/nextstep(). */
    hall=10000;hall_input(0);
    if(!setjmp(fault_return)){nextstep();assert(0);}
    assert(!gate_on());
    hall_input(7);
    if(!setjmp(fault_return)){nextstep();assert(0);}
    assert(!gate_on());
    puts("PASS production Hall GPIO/EQEP0 ISR -> Rel17 mapping/filter/timeout/hybrid -> MCPWM; IRQ timestamp latency unqualified");
}
#endif
int main(void)
{
    /* Anonymous virtual register RAM. Absolutely no device access. */
    void *r=mmap((void*)0x40000000,0x07000000,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(r==(void*)0x40000000);
    SYSCTL->SOCLOCK.MCLKCFG=DL_SYSCTL_MCLK_DIV_2_DIV_4;
    DL_GPIO_enablePower(GPIO1);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PB15);
    *(volatile uint32_t *)(uintptr_t)&GPIO1->DIN31_0=DL_GPIO_PIN(15U);
    assert(!setjmp(fault_return));
    am13e_app_motor_init();
    assert((MCPWM0->TBCTL&MCPWM_TBCTL_CLKDIV_MASK)==
        (uint32_t)DL_MCPWM_CLOCK_DIVIDER_2<<MCPWM_TBCTL_CLKDIV_OFS);
    assert(am13e_app_motor_inactive_aq_boot_preflight());
    am13e_app_motor_bemf_init();
#ifdef PWM_ENABLE
    assert((MCPWM0->TZCTL&MCPWM_TZCTL_TZB_MASK)==
        ((uint32_t)DL_MCPWM_TZ_ACTION_HIGH<<MCPWM_TZCTL_TZB_OFS));
    checkcfg();assert(cfg.damp==1);
#endif
    for(unsigned n=0;n<18U;++n){ECAP1->TSCTR+=12500U;am13e_app_motor_bemf_tick();}
    assert(am13e_app_motor_inactive_preflight_ok());
#ifdef HALL_MAP
    hall_runtime();assert(munmap(r,0x07000000)==0);return 0;
#endif
    cfg.damp=1;cfg.sine_power=8;ertm=6000;ival=3000;sync=6;step=0;reverse=0;
    am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);
    nextstep();start();carrier();
    assert(MCPWM0->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS &&
        MCPWM0->DBFED==(AM13E_MOTOR_OUTPUT_FED_TICKS?AM13E_MOTOR_OUTPUT_FED_TICKS:1U));
    inject_shadow_zero=1U;
    for(unsigned n=0;n<12U;++n){nextstep();zero();expect_sixstep();carrier();assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));}
    reverse=1;for(unsigned n=0;n<12U;++n){nextstep();zero();expect_sixstep();carrier();}
#ifdef SW_BLANKING
    /* Real ZERO/CMPC IRQ -> ECAP capture gating, elapsed counter intact.
     * Coalesced old-CMPC/new-ZERO events must leave capture blanked. */
    const uint32_t elapsed=ECAP1->TSCTR;
    zero();assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    const int before_blank=ival;
    ECAP1->CAP1=500000U;ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;
    ECAP1_IRQHandler();assert(ival==before_blank);
    MCPWM0->TBCTR=10U;MCPWM0->INTFLAG=DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2;
    MCPWM0_IRQHandler();assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    MCPWM0->TBCTR=AM13E_BEMF_BLANK_TICKS;MCPWM0->INTFLAG=DL_MCPWM_INT_ET_2;
    MCPWM0_IRQHandler();assert(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK);
    assert(ECAP1->TSCTR==elapsed);
#endif
    /* Empty vectors after replacement must preserve the new hardware owner. */
    const int before_empty=step;
    MCPWM0->INTFLAG=0U;MCPWM0_IRQHandler();assert(gate_on());
    ECAP1->ECFLG=0U;ECAP1_IRQHandler();
    am13e_app_motor_bemf_commutation_delay_us(1234);
    *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=0U;
    TIMG12_0_IRQHandler();assert(step==before_empty && gate_on());
    *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
    TIMG12_0_IRQHandler();zero();carrier();assert(step!=before_empty);
    /* Actual CMPSS phase route -> ECAP1 -> Core BEMF policy -> TIMG12 ISR. */
    cfg.timing=16;ival=3000;ertm=6000;const int previous_step=step;
    capture(0U);capture(1U); /* quantized zero must be an early rejection, not fault */
    assert(ival==3000 && step==previous_step);
    capture(100000U); /* 500us < original ival/2: no schedule/step. */
    assert(ival==3000 && step==previous_step);
    capture(400000U); /* 2000us: accepted timing advance schedule. */
    assert(ival==2750 && step==previous_step);
    *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
    TIMG12_0_IRQHandler();zero();carrier();assert(step!=previous_step);
    /* Missing-edge timeout enters original Rel17 sync reset, without
     * inventing an additional AM13E output-enable policy. */
    const int timed_out_step=step;
    ECAP1->TSCTR=8000000U;am13e_app_motor_bemf_tick();
    assert(sync==0 && fast==0 && ival==10000 && ertm==100000000);
    TIMG12_0_IRQHandler();assert(step==timed_out_step && gate_on());
    /* Real accepted captures acquire six crossings of synchronization;
     * the original six-interval check also detects a desynchronized rotor. */
    ival=3000;ertm=6000;sync=0;
    nextstep();zero();carrier();
    for(unsigned crossing=0;crossing<6U;++crossing){
        capture(400000U);assert(sync==crossing+1U);
        *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
        TIMG12_0_IRQHandler();zero();carrier();
    }
    assert(sync==6);
    ival=500;ertm=1500;sync=6;fast=1;
    for(unsigned n=0;n<6U;++n){nextstep();zero();carrier();}
    assert(sync==0 && fast==0 && ival==5000 && ertm==100000000);
    /* Normal duty/frequency changes remain coherent at every write boundary. */
    for(unsigned duty=0;duty<=2000U;duty+=100U){
        am13e_app_motor_pwm_apply(duty,16,96,ertm,1,0,0,1);zero();carrier();
    }
    assert(shadow_boundaries>100U);
    am13e_app_motor_commutation_enable(0);
    assert(!gate_on() && am13e_app_motor_inactive_preflight_ok());
    /* Original first-sine sample and native output lifecycle. */
    sine=1000;step=0;prep=0;reverse=0;nextstep();start();
    for(unsigned n=0;n<360U;++n){nextstep();zero();carrier();}
    /* Original sine-to-sixstep rounding/handoff through laststep(). */
    am13e_app_motor_commutation_enable(0);prep=1;lock=0;laststep();
    assert(!sine && !prep && !step && oldstep>=1 && oldstep<=6);
    am13e_app_motor_pwm_apply(300,24,48,0,1,0,0,0);
    am13e_app_motor_brake_counter_update(0,0,0,0,300);
    zero();zero();carrier();assert(gate_on());
    am13e_app_motor_brake_counter_update(0,0,0,0,0);
    assert(!gate_on());
    const int stopped_step=step;
    *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
    TIMG12_0_IRQHandler();ECAP1_IRQHandler();
    MCPWM0->INTFLAG=DL_MCPWM_INT_ET_1;MCPWM0_IRQHandler();
    assert(step==stopped_step && !gate_on());
    /* Original lock laststep and ZTC coast, then native brushed COM output. */
    for(int hold=1;hold<=2;++hold){
        lock=hold;cfg.throt_ztc=0;laststep();
        am13e_app_motor_pwm_apply(500,24,48,0,1,hold,0,0);
        am13e_app_motor_brake_counter_update(0,0,0,hold,500);
        zero();zero();carrier();assert(gate_on());
        am13e_app_motor_commutation_enable(0);
    }
    cfg.throt_ztc=1;throt=0;laststep();
    am13e_app_motor_brake_counter_update(0,0,0,lock,500);assert(!gate_on());
    cfg.throt_ztc=0;lock=0;
    for(int dir=0;dir<=1;++dir)for(int damp=0;damp<=1;++damp){
        am13e_app_motor_pwm_apply(900,24,48,0,damp,0,1,1);
        am13e_app_motor_brushed_write(dir,damp);start();carrier();
        am13e_app_motor_commutation_enable(0);assert(!gate_on());
    }
    /* Actual audio hardware ownership, clock/dead-time scaling and release. */
    for(int mode=1;mode<=2;++mode){
        am13e_app_motor_audio_begin(mode);
        assert((MCPWM0->TBCTL&MCPWM_TBCTL_CLKDIV_MASK)==
            (uint32_t)(mode==1?DL_MCPWM_CLOCK_DIVIDER_32:DL_MCPWM_CLOCK_DIVIDER_2)<<MCPWM_TBCTL_CLKDIV_OFS);
        assert(MCPWM0->DBRED==(mode==1?(AM13E_MOTOR_OUTPUT_RED_TICKS+15U)/16U:AM13E_MOTOR_OUTPUT_RED_TICKS));
#ifdef PWM_ENABLE
        assert(MCPWM0->DBFED==MCPWM0->DBRED);
#endif
        am13e_app_motor_audio_period(2000U);
        am13e_app_motor_audio_compare(400U,700U);zero();zero();carrier();assert(gate_on());
        am13e_app_motor_audio_compare(300U,500U);zero();carrier();
        am13e_app_motor_audio_period(1800U);assert(!gate_on());
        am13e_app_motor_audio_compare(0U,400U);zero();zero();carrier();
        am13e_app_motor_audio_end();assert(!gate_on());
        assert(am13e_app_motor_inactive_preflight_ok());
        assert(MCPWM0->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS);
    }
    /* Actual music adapter: constant-tone CCR>ARR and sticky UIF behavior
     * after a missed period, even when counter order does not show wrap. */
    am13e_app_audio_music_begin();zero();zero();
    am13e_app_audio_music_note(0,10,255);zero();zero();carrier();
    assert(MCPWM0->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    assert(MCPWM0->PWM1_AQCTLB==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
    assert((MCPWM0->ETSEL&MCPWM_ETSEL_ET2_SEL_MASK)==
        (uint32_t)DL_MCPWM_EVT_TBCTR_ZERO<<MCPWM_ETSEL_ET2_SEL_OFS);
    MCPWM0->TBCTR=5U;MCPWM0->INTFLAG|=DL_MCPWM_INT_ET_2;
    MCPWM0_IRQHandler();
    assert(!(MCPWM0->INTEN&DL_MCPWM_INT_ET_2));
    assert(MCPWM0->INTFLAG&DL_MCPWM_INT_ET_2);
    MCPWM0->ETCNT=1U<<MCPWM_ETCNT_ET2_CNT_OFS; /* queued extra old period */
    am13e_app_audio_music_tick();
    assert(DL_MCPWM_getEventTriggerEventCount(MCPWM0,DL_MCPWM_ET_2)==0U);
    zero();carrier();
    assert(MCPWM0->PWM3_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    assert(MCPWM0->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
    assert(!(MCPWM0->INTFLAG&DL_MCPWM_INT_ET_2));
    am13e_app_audio_music_tick(); /* no new ZERO: no second swap */
    zero();carrier();assert(MCPWM0->PWM3_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    am13e_app_audio_music_pause();zero();carrier();
    am13e_app_audio_end();assert(!gate_on());
    /* Maximum 16-bit carrier image: inactive/freewheel compares must not
     * overflow merely because the electrical DT offset is added. */
    {
        const uint16_t image[6]={DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA,
            DL_MCPWM_AQ_OUTPUT_LOW_ZERO|DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB,0,DL_MCPWM_AQ_OUTPUT_HIGH_ZERO,0,0};
        const uint16_t maximum[6]={65535,65535,65535,65535,65535,65535};
        am13e_motor_output_stage(image,maximum,65534U);
        assert(MCPWM0->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
        assert(MCPWM0->PWM1_AQCTLB==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
        assert(MCPWM0->PWM1_CMPA==1U && MCPWM0->PWM1_CMPB==1U);
        assert(!gate_on());
    }
    /* A trip can latch halfway through masked SHADOW writes. Test in
     * an isolated anonymous-RAM child because fault reset never returns. */
    for(volatile unsigned fault_case=0;fault_case<2U;++fault_case){
    pid_t child=fork();assert(child>=0);
    if(child==0){
        sine=0;step=0;lock=0;nextstep();
        am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);start();carrier();
        test_primask=0U;
        if(fault_case){
            am13e_app_motor_commutation_enable(0);
            am13e_app_audio_music_begin();zero();zero();
            am13e_app_audio_music_note(0,0,50);zero();zero();carrier();
        }
        if(!setjmp(fault_return)){
            if(fault_case){
                inject_ack_trip=1U;MCPWM0->INTFLAG=DL_MCPWM_INT_ET_2;
            }else{
                inject_trip=1U;
                am13e_app_motor_pwm_apply(1000,24,48,ertm,1,0,0,1);
                assert(test_primask==0U && (MCPWM0->TZCBCOSTFLAG&DL_MCPWM_TZ_FLAG_OST_TZ1));
            }
            MCPWM0_IRQHandler();assert(0);
        }
        assert(!gate_on() && watchdog_arms==1U);
        assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
        assert(!DL_MCPWM_getGlobalLoadOneShotLatchStatus(MCPWM0));
        _exit(0);
    }
    int child_status;assert(waitpid(child,&child_status,0)==child);
    assert(WIFEXITED(child_status) && WEXITSTATUS(child_status)==0);
    }
    /* Fault dominates a pending startup ZERO and cannot re-enable pads. */
    sine=0;step=0;lock=0;nextstep();am13e_app_motor_commutation_commit();
    am13e_app_motor_commutation_enable(1);
    if(!setjmp(fault_return)){
        MCPWM0->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ1;
        MCPWM0->INTFLAG=DL_MCPWM_INT_TZ_OST|DL_MCPWM_INT_ET_1;
        MCPWM0_IRQHandler();assert(0);
    }
    assert(watchdog_arms==1U && !gate_on());
    assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
    assert(MCPWM0->TZCBCOSTFLAG&DL_MCPWM_TZ_FLAG_OST_TZ1);
    if(!setjmp(fault_return)){am13e_app_motor_commutation_enable(1);assert(0);}
    assert(!gate_on());
    assert(!(MCPWM0->INTEN&DL_MCPWM_INT_ET_1));
    assert(munmap(r,0x07000000)==0);
    puts("PASS original Core -> native MCPWM/pinmux/gate, sine/sixstep/reverse/brake and dominant OST shutdown");
}
