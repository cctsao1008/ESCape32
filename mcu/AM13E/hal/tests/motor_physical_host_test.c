#include "motor_audio_hw.h"
/* Original Core + native HAL + actual SDK registers/DriverLib. Only core
 * primitives, GPIO aliases and autonomous register loads are emulated. */
#include <sys/mman.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../../../../src/main.c"
#include "motor_safety.h"
#include "motor_output_runtime.h"
#include "board_motor_output_provider.h"
#include "motor_bemf.h"
#include "motor_event_timer.h"
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
uint32_t NVIC_GetVector(IRQn_Type i){(void)i;return 0U;}
int am13e_app_io_watchdog_fault_arm(void){++watchdog_arms;return 1;}
void test_global_load(MCPWM_Regs *p)
{
    if(!(p->GLDOSHTSTS&MCPWM_GLDOSHTSTS_OSHTLATCH_MASK))return;
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

static unsigned inject_shadow_zero,shadow_boundaries;
static uint32_t active_sum(MCPWM_Regs *p)
{
    return p->TBPRD+p->PWM1_CMPA+p->PWM1_CMPB+p->PWM2_CMPA+p->PWM2_CMPB+
        p->PWM3_CMPA+p->PWM3_CMPB+p->PWM1_AQCTLA+p->PWM1_AQCTLB+
        p->PWM2_AQCTLA+p->PWM2_AQCTLB+p->PWM3_AQCTLA+p->PWM3_AQCTLB;
}
void test_shadow_written(MCPWM_Regs *p)
{
    if(!inject_shadow_zero)return;
    const uint32_t before=active_sum(p);
    test_global_load(p); /* ZERO during every individual register write. */
    assert(before==active_sum(p));++shadow_boundaries;
}
/* TI AQ/RED register behavior observer, not a commutation controller.
 * Core chooses phases; the observer checks the resulting logical gate
 * levels at each TBCLK. This is model evidence, not a pin measurement. */
static unsigned aq_state[6],gate_state[6];
static uint64_t virtual_tick,red_deadline[3],last_off[6];
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
    assert(MCPWM0->DBFED==0U);
    for(unsigned tick=0;tick<=MCPWM0->TBPRD;++tick,++virtual_tick){
        for(unsigned phase=0;phase<3U;++phase){
            const unsigned a=phase*2U,b=a+1U,old_a=aq_state[a];
            const unsigned previous[2]={gate_state[a],gate_state[b]};
            if(tick==0U){aq_event(a,aq[a]&3U);aq_event(b,aq[b]&3U);}
            if(tick==cc[a]){aq_event(a,(aq[a]>>4U)&3U);aq_event(b,(aq[b]>>4U)&3U);}
            if(tick==cc[b]){aq_event(a,(aq[a]>>8U)&3U);aq_event(b,(aq[b]>>8U)&3U);}
            if(!aq_state[a]){gate_state[a]=0U;red_deadline[phase]=UINT64_MAX;}
            else if(!old_a)red_deadline[phase]=virtual_tick+red;
            if(aq_state[a] && virtual_tick==red_deadline[phase])gate_state[a]=1U;
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
        if(phase==negative[step-1]){
            assert(aq[phase*2U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
            assert(aq[phase*2U+1U]==DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
        }else if(phase!=positive[step-1]){
            assert(aq[phase*2U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
            assert(aq[phase*2U+1U]==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
        }
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
    if(MCPWM0->INTEN&DL_MCPWM_INT_ET_1){
        uint32_t mask=test_primask;test_primask=0U;
        MCPWM0->INTFLAG=DL_MCPWM_INT_ET_1;MCPWM0_IRQHandler();test_primask=mask;
    }
}
static int gate_on(void)
{ return ((GPIO1->DOUT31_0&DL_GPIO_PIN(13U))!=0U)==(AM13E_MOTOR_GATE_ACTIVE_LEVEL!=0); }
static void start(void)
{
    am13e_app_motor_commutation_commit();
    am13e_app_motor_commutation_enable(1);
    assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));
    assert(!gate_on());
    assert(!MCPWM0->PWM1_AQSFRC && !MCPWM0->PWM2_AQSFRC && !MCPWM0->PWM3_AQSFRC);
    zero();assert(!gate_on());
    zero();assert(gate_on());
}
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
    for(unsigned n=0;n<18U;++n){ECAP1->TSCTR+=12500U;am13e_app_motor_bemf_tick();}
    assert(am13e_app_motor_inactive_preflight_ok());
    cfg.damp=1;cfg.sine_power=8;ertm=6000;ival=3000;sync=6;step=0;reverse=0;
    am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);
    nextstep();start();carrier();
    assert(MCPWM0->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS && MCPWM0->DBFED==0U);
    inject_shadow_zero=1U;
    for(unsigned n=0;n<12U;++n){nextstep();zero();expect_sixstep();carrier();assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));}
    reverse=1;for(unsigned n=0;n<12U;++n){nextstep();zero();expect_sixstep();carrier();}
    /* Actual CMPSS phase route -> ECAP1 -> Core BEMF policy -> TIMG12 ISR. */
    cfg.timing=16;ival=3000;ertm=6000;const int previous_step=step;
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
        am13e_app_motor_audio_period(2000U);
        am13e_app_motor_audio_compare(400U,700U);zero();zero();carrier();assert(gate_on());
        am13e_app_motor_audio_compare(300U,500U);zero();carrier();
        am13e_app_motor_audio_period(1800U);assert(!gate_on());
        am13e_app_motor_audio_compare(0U,400U);zero();zero();carrier();
        am13e_app_motor_audio_end();assert(!gate_on());
        assert(am13e_app_motor_inactive_preflight_ok());
        assert(MCPWM0->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS);
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
