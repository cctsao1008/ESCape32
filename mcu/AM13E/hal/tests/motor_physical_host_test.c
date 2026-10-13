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
#include "motor_power_stage.h"
#include "board_motor_output_provider.h"
#include "motor_bemf.h"
#include "motor_event_timer.h"
#include "motor_hall.h"
#ifdef HALL_MAP
#include <dl_eqep.h>
#endif

#include <dl_mcpwm.h>
#include <dl_sysctl.h>
#include <dl_xbar.h>
#include <dl_cmpss_lite.h>
#include <dl_gpio.h>
#include <dl_ecap.h>
#include <dl_timer.h>
#if defined(AM13E_FATAL_PATH_FIXTURE) || defined(AM13E_SC5_FIXTURE)
#include "../analog_runtime.c"
#endif
#ifdef AM13E_SC5_FIXTURE
void DL_Common_delayCycles(uint32_t cycles) { (void)cycles; }
static void sc5_adc(void)
{
    test_primask=1U;am13e_app_adc_init();
    assert(SYSCTL->SOCLOCK.ANAREFCTL==DL_SYSCTL_VREF_EXTERNAL_FULL_SCALE);
    AM13E_BemfReference ref;AM13E_AdcRaw raw;
    assert(!am13e_app_adc_get_raw(&raw) && !am13e_app_adc_get_reference(&ref));
    ADC_Regs *adcs[]={ADC0,ADC1,ADC2};
    ADC_RESULT_Regs *results[]={ADC0RESULT,ADC1RESULT,ADC2RESULT};
    for(unsigned i=0;i<3U;++i){
        for(unsigned j=0;j<3U;++j)
            *(volatile uint16_t *)((uintptr_t)results[i]+ADC_ADCRESULT0+j*DL_ADCRESULT_STEP)=2048U;
        adcs[i]->ADCINTFLG=ADC_ADCINTFLG_ADCINT1_MASK|ADC_ADCINTFLG_ADCINT1RESULT_MASK;
    }
    tick=UINT32_MAX-3U;adctrig();adctrig();
    ADC2_INT1_IRQHandler();ADC0_INT1_IRQHandler();
    assert(!am13e_app_adc_get_raw(&raw));
    tick=2U;ADC1_INT1_IRQHandler();adctrig();
    assert(am13e_app_adc_get_reference(&ref));
    assert(ref.sample==1U && ref.acquisition_tick==UINT32_MAX-3U && ref.completion_tick==2U);
    assert(ref.code[0]==775U && ref.code[1]==620U && ref.code[2]==495U);
    tick=40U;assert(!am13e_app_adc_get_reference(&ref));
    adc_latest.sample_count=UINT32_MAX;adc_sample_seq=UINT32_MAX-1U;
    ADC0_INT1_IRQHandler();ADC1_INT1_IRQHandler();ADC2_INT1_IRQHandler();
    assert(am13e_app_adc_get_raw(&raw) && raw.sample_count==0U && adc_sample_seq==0U);
    /* Completion cannot freshen an acquisition that began too long ago. */
    adctrig();assert(!am13e_app_adc_get_reference(&ref));
    tick=41U;ADC0_INT1_IRQHandler();ADC1_INT1_IRQHandler();ADC2_INT1_IRQHandler();adctrig();
    assert(am13e_app_adc_get_reference(&ref) && ref.sample==1U);
    adc_sample_seq=1U;assert(!am13e_app_adc_get_raw(&raw));adc_sample_seq=0U;
    puts("PASS SC5 actual ADC reference, coherent freshness, busy skips and sequence/time wrap");
}
#endif
#ifdef AM13E_FATAL_PATH_FIXTURE
/* Run fatal branches from the actual ADC/UART/auxiliary units against the
 * SAME configured motor backend, not mocked shutdown functions. */
#define initialized serial_fixture_initialized
#define am13e_app_io_cli_line fixture_cli_result
#include "../serial_uart_rx.c"
#undef am13e_app_io_cli_line
#undef initialized
#define initialized aux_fixture_initialized
#include "../aux_gpio.c"
#undef initialized
#define main fixture_returned_main
#define am13e_rel17_app_main fixture_returning_core
int fixture_returned_main(void);
#include "../../config.c"
#undef am13e_rel17_app_main
#undef main
void fixture_returning_core(void) {}
unsigned fixture_uart_rx_pending;
unsigned fixture_gpio_power_failure;
static int cli_fault_result;
int fixture_cli_result(char *line) { (void)line;return cli_fault_result; }
void DL_Common_delayCycles(uint32_t cycles) { (void)cycles; }
void am13e_app_io_watchdog_feed(void) { assert(0); }
void am13e_app_io_watchdog_arm(void) { assert(0); }
bool am13e_app_pb14_service_takeover(void) { return true; }
int am13e_pb14_bidir_tx_busy(void) { return 0; }
void am13e_app_service_reset(void) { assert(0); }
#endif
uint32_t test_primask=1U;
TestSCB test_scb;
static unsigned enabled[80],priority[80],watchdog_arms;
static jmp_buf fault_return;
void test_halt(void){longjmp(fault_return,1);}
#ifndef AM13E_FATAL_PATH_FIXTURE
void __error__(const char *file,uint32_t line){fprintf(stderr,"SDK assertion %s:%u\n",file,line);abort();}
#endif
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
int am13e_app_io_watchdog_fault_arm(void)
{
#ifdef AM13E_FATAL_PATH_FIXTURE
    /* Reset supervision is reached only AFTER the real bridge shutdown. */
    assert(test_primask==1U);
    assert(!(GPIO1->DOE31_0&AM13E_GATE_MASK) ||
        (((GPIO1->DOUT31_0&AM13E_GATE_MASK)!=0U)!=(AM13E_MOTOR_GATE_ACTIVE_LEVEL!=0)));
    if(fixture_gpio_power_failure){
        const uint32_t low=(uint32_t)DL_MCPWM_AQ_SW_CONTINUOUS_LOW*17U;
        assert(MCPWM4->PWM1_AQSFRC==low && MCPWM4->PWM2_AQSFRC==low && MCPWM4->PWM3_AQSFRC==low);
    }else assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
    assert((MCPWM4->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)==DL_MCPWM_COUNTER_MODE_STOP_FREEZE);
    assert(!DL_MCPWM_getGlobalLoadOneShotLatchStatus(MCPWM4));
#endif
    ++watchdog_arms;return 1;
}
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
        p->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ2;
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
        MCPWM4->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ2;
        MCPWM4->INTFLAG|=DL_MCPWM_INT_TZ_OST;inject_trip=0U;
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
    assert(p==MCPWM4 && (unsigned)o<6U);
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
    uint32_t aq[6]={MCPWM4->PWM1_AQCTLA,MCPWM4->PWM1_AQCTLB,
        MCPWM4->PWM2_AQCTLA,MCPWM4->PWM2_AQCTLB,MCPWM4->PWM3_AQCTLA,MCPWM4->PWM3_AQCTLB};
    uint32_t cc[6]={MCPWM4->PWM1_CMPA,MCPWM4->PWM1_CMPB,
        MCPWM4->PWM2_CMPA,MCPWM4->PWM2_CMPB,MCPWM4->PWM3_CMPA,MCPWM4->PWM3_CMPB};
    const unsigned red=MCPWM4->DBRED;
#ifdef PWM_ENABLE
    assert(MCPWM4->DBFED==red && (MCPWM4->DBCTL&MCPWM_DBCTL_OUT_MODE_MASK)==3U);
#else
    assert(MCPWM4->DBFED==1U && (MCPWM4->DBCTL&MCPWM_DBCTL_OUT_MODE_MASK)==(1U<<DL_MCPWM_DB_RED));
#endif
    for(unsigned tick=0;tick<=MCPWM4->TBPRD;++tick,++virtual_tick){
        MCPWM4->TBCTR=tick;
#ifdef SW_BLANKING
        if(tick==AM13E_BEMF_BLANK_TICKS && (MCPWM4->INTEN&DL_MCPWM_INT_ET_2)){
            MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;MCPWM4_IRQHandler();
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
                if(fed_deadline[phase]==UINT64_MAX)fed_deadline[phase]=virtual_tick+MCPWM4->DBFED;
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
    const uint32_t aq[6]={MCPWM4->PWM1_AQCTLA,MCPWM4->PWM1_AQCTLB,
        MCPWM4->PWM2_AQCTLA,MCPWM4->PWM2_AQCTLB,MCPWM4->PWM3_AQCTLA,MCPWM4->PWM3_AQCTLB};
    assert(step>=1 && step<=6);
    const unsigned floating=3U-positive[step-1]-negative[step-1];
    assert((SYSCTL->SOCLOCK.CMPHPMXSEL&SYSCTL_CMPHPMXSEL_CMP0HPMXSEL_MASK)==floating);
    /* Independent analog relation: G431 sees reference>phase, optionally
     * inverted, while native CMPSS sees phase>reference. */
    const unsigned rising=((step&1)!=0)^(reverse!=0);
    const unsigned native_falling=(ECAP1->ECCTL1&ECAP_ECCTL1_CAP1POL_MASK)!=0U;
    for(unsigned before=0;before<2U;++before){
        const unsigned after=!before;
        const unsigned g431_before=(!before)^rising,g431_after=(!after)^rising;
        const unsigned original_capture=!g431_before && g431_after;
        const unsigned native_capture=native_falling?(before&&!after):(!before&&after);
        assert(original_capture==native_capture);
    }
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
static uint32_t capture_origin;
void test_ecap_counter_started(ECAP_Regs *p,int was_running)
{
    if(!was_running)capture_origin=p->TSCTR;
}
static void capture(unsigned ticks)
{
    const unsigned enabled=ECAP1->ECEINT;
    ECAP1->TSCTR=capture_origin+ticks;
    ECAP1->CAP1=ECAP1->TSCTR;ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;
    ECAP1_IRQHandler();
    if(enabled && !ECAP1->ECEINT)capture_origin=ECAP1->CAP1;
}
static void zero(void)
{
    test_global_load(MCPWM4);
    uint32_t events=DL_MCPWM_INT_ET_1;
    MCPWM4->TBCTR=0U;
    if(MCPWM4->INTEN&events){
        uint32_t mask=test_primask;test_primask=0U;
        MCPWM4->INTFLAG=MCPWM4->INTEN&events;MCPWM4_IRQHandler();test_primask=mask;
    }
}
static void step_publication(void)
{
    uint32_t before[16],after[16];active_image(MCPWM4,before);
    const int previous=step;
    nextstep();
    assert(step==(reverse?(previous>1?previous-1:6):(previous<6?previous+1:1)));
    assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    assert(!am13e_app_motor_bemf_reference_set(1024U)); /* Pending is not inactive. */
    active_image(MCPWM4,after);assert(memcmp(before,after,sizeof before)==0);
    /* Already queued ZERO cannot publish a still-armed shadow image. */
    MCPWM4->INTFLAG=DL_MCPWM_INT_ET_1;MCPWM4_IRQHandler();
    assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;ECAP1_IRQHandler();
    zero();expect_sixstep();carrier();
}
static int gate_on(void)
{ return ((GPIO1->DOUT31_0&DL_GPIO_PIN(13U))!=0U)==(AM13E_MOTOR_GATE_ACTIVE_LEVEL!=0); }
static void start(void)
{
    am13e_app_motor_commutation_commit();
    MCPWM4->ETCNT|=1U<<MCPWM_ETCNT_ET1_CNT_OFS; /* queued pre-stop ZERO */
    am13e_app_motor_commutation_enable(1);
    assert(DL_MCPWM_getEventTriggerEventCount(MCPWM4,DL_MCPWM_ET_1)==0U);
    assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));
    assert(!gate_on());
    assert(!MCPWM4->PWM1_AQSFRC && !MCPWM4->PWM2_AQSFRC && !MCPWM4->PWM3_AQSFRC);
    zero();assert(!gate_on());
    zero();assert(gate_on());
}
static void calibration_tick(void)
{
    /* Elapsed wall time advances TSCTR only when native counter runs. */
    if(ECAP1->ECCTL2&ECAP_ECCTL2_TSCTRSTOP_MASK)ECAP1->TSCTR+=12500U;
    am13e_app_motor_bemf_tick();
}
static void calibration_lifecycle(void)
{
    /* Fork preserves the same real initialized production state while each
     * case begins before calibration. No production reset/test hook. */
    static const unsigned entry_ticks[]={0U,1U,8U,16U};
    for(volatile unsigned mode=0U;mode<6U;++mode){
        pid_t child=fork();assert(child>=0);
        if(child==0){
            if(setjmp(fault_return))assert(!"unexpected calibration fault");
            const unsigned early=mode<4U?entry_ticks[mode]:0U;
            for(unsigned i=0;i<early;++i){calibration_tick();}
            if(mode<4U){
                resetcom();
                assert(!(ECAP1->ECCTL2&ECAP_ECCTL2_TSCTRSTOP_MASK));
                am13e_app_motor_audio_begin(1);
                for(unsigned i=0;i<4U;++i){calibration_tick();}
                am13e_app_motor_audio_end();
                resetcom(); /* Cancel this incomplete second window too. */
            }
            if(mode==4U){
                am13e_app_motor_fault_shutdown();
                for(unsigned i=0;i<20U;++i){calibration_tick();}
                assert(!(ECAP1->ECCTL2&ECAP_ECCTL2_TSCTRSTOP_MASK));
                assert(!gate_on() && !(GPIO0->DOE31_0&(DL_GPIO_PIN(8U)|DL_GPIO_PIN(21U)|DL_GPIO_PIN(24U)|DL_GPIO_PIN(2U)|DL_GPIO_PIN(10U)|DL_GPIO_PIN(25U))));
                _exit(0);
            }
            /* First tick establishes the fresh origin; sixteen COMPLETE
             * intervals establish 200MHz. A shortened estimate would fail
             * the actual Core interval/advance checks below. */
            for(unsigned i=0;i<17U;++i){calibration_tick();}
            test_primask=1U;compctl(1);
            am13e_app_motor_bemf_interval_select(18000);
            const uint32_t epoch=ECAP1->TSCTR;
            cfg.timing=16;sync=fast=0;ertm=18000;
#ifdef HALL_MAP
            ival=6000;
#else
            ival=3000;
#endif
#ifdef SW_BLANKING
            MCPWM4->TBCTR=AM13E_BEMF_BLANK_TICKS;
            MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;MCPWM4_IRQHandler();
#endif
            ECAP1->CAP1=epoch+600000U;ECAP1->TSCTR=ECAP1->CAP1;
            ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;ECAP1_IRQHandler();
#ifdef HALL_MAP
            assert(ival==6000);
#else
            assert(ival==3000);
#endif
            assert(TIMG12->COUNTERREGS.LOAD==75000U-1U);
            _exit(0);
        }
        int status;assert(waitpid(child,&status,0)==child);
        assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    puts("PASS calibration: early audio/reset cancellation, fresh 16-interval restart, uninterrupted window and latched fault stop");
}
#ifndef HALL_MAP
static void absolute_capture(uint32_t stamp,uint16_t extra)
{
    ECAP1->TSCTR=stamp;ECAP1->CAP1=stamp;
    ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1|extra;
    ECAP1_IRQHandler();
}
static void timing_epochs(void)
{
    am13e_app_motor_commutation_enable(0);
    sine=prep=0;step=0;reverse=0;sync=fast=0;ival=3000;ertm=18000;
    cfg.timing=16;cfg.damp=1;
    uint32_t accepted=UINT32_C(0xfff00000);
    ECAP1->TSCTR=accepted;
    am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);
    nextstep();start();
    /* Independently advancing hardware time; CAP1 is absolute, not a
     * supplied decoded interval. The second crossing spans 32-bit wrap. */
    static const unsigned intervals[]={3000,3000,2600,3400,3000};
    static const int expected_ival[]={3000,3000,2900,3025,3018};
    static const unsigned expected_delay[]={750,750,725,756,754};
    for(unsigned n=0;n<5U;++n){
#ifdef SW_BLANKING
        MCPWM4->TBCTR=AM13E_BEMF_BLANK_TICKS;
        MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;MCPWM4_IRQHandler();
#endif
        if(n==4U){
            const int before=ival;
            absolute_capture(accepted+100000U,0U);
            absolute_capture(accepted+200000U,0U);
            assert(ival==before); /* Neither rejected edge moves the epoch. */
        }
        const uint32_t next=accepted+intervals[n]*200U;
        absolute_capture(next,next<accepted?DL_ECAP_ISR_SOURCE_CTROVF:0U);
        assert(ival==expected_ival[n]);
        assert(TIMG12->COUNTERREGS.LOAD==expected_delay[n]*100U-1U);
        accepted=next;
        /* Timer expiration, followed by independently delayed carrier ZERO.
         * Commutation must not reset or stop the accepted-crossing clock. */
        ECAP1->TSCTR=accepted+expected_delay[n]*200U;
        *(volatile uint32_t *)(uintptr_t)&TIMG12->CPU_INT.MIS=DL_TIMER_INTERRUPT_ZERO_EVENT;
        TIMG12_0_IRQHandler();
        assert(ECAP1->TSCTR==accepted+expected_delay[n]*200U);
        ECAP1->TSCTR+=1200U; /* 6us publication latency, not compensation. */
        const uint32_t published=ECAP1->TSCTR;
        zero();assert(ECAP1->TSCTR==published);expect_sixstep();
        if(n==2U){reverse=1;step_publication();} /* Epoch survives reversal. */
    }
    ECAP1->TSCTR=accepted+AM13E_BEMF_REL17_TIMEOUT_US*200U;
    am13e_app_motor_bemf_tick();assert(!sync && ival==10000);
    am13e_app_motor_commutation_enable(0);
    ECAP1->TSCTR=100U;step=0;reverse=0;ival=3000;ertm=18000;
    nextstep();start();
#ifdef SW_BLANKING
    MCPWM4->TBCTR=AM13E_BEMF_BLANK_TICKS;
    MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;MCPWM4_IRQHandler();
#endif
    absolute_capture(600100U,0U);assert(ival==3000);
    am13e_app_motor_commutation_enable(0);
    puts("PASS accepted-crossing epochs: speed changes, rejection, wrap, delayed ZERO, reversal, timeout and restart");
}
#endif
#ifdef AM13E_FATAL_PATH_FIXTURE
static void fatal_paths(int early)
{
    const unsigned count=early?3U:16U;
    for(volatile unsigned n=0;n<count;++n){
        pid_t child=fork();assert(child>=0);
        if(child==0){
            watchdog_arms=0U;
            if(!early){
                assert(gate_on());
                am13e_app_motor_bemf_commutation_delay_us(20);
            }
            test_primask=early?1U:0U;
            if(!setjmp(fault_return)){
                if(early){
                    if(n==0U)ADC0_INT1_IRQHandler();
                    else if(n==1U)RX_HANDLER();
                    else {test_primask=0U;am13e_app_motor_init();}
                }else switch(n){
                    case 0:adc_initialized=0U;adctrig();break;
                    case 1:adc_initialized=1U;adc_inflight=0U;ADC0_INT1_IRQHandler();break;
                    case 2:
                        adc_initialized=adc_inflight=1U;
                        ADC0->ADCINTFLG=ADC_ADCINTFLG_ADCINT1_MASK|ADC_ADCINTFLG_ADCINT1RESULT_MASK;
                        ADC1->ADCINTFLG=ADC2->ADCINTFLG=ADC0->ADCINTFLG;
                        completed=0U;
                        *(volatile uint16_t *)((uintptr_t)ADC2RESULT+ADC_ADCRESULT0+2U*DL_ADCRESULT_STEP)=4095U;
                        ADC1_INT1_IRQHandler();ADC2_INT1_IRQHandler();
                        ADC0_INT1_IRQHandler();break; /* exceeds synthetic profile fullscale */
                    case 3:serial_fixture_initialized=0U;RX_HANDLER();break;
                    case 4:
                        serial_fixture_initialized=1U;crsf_tx_available=true;
                        *(volatile uint32_t *)(uintptr_t)&RX_UC->uart->CPU_INT.IIDX=DL_UART_IIDX_EOT_DONE;
                        RX_HANDLER();break;
                    case 5:case 6:
                        cli_active=true;am13e_cli_line_init(&cli_rx);
                        assert(am13e_cli_line_byte(&cli_rx,'\n')==1);
                        cli_fault_result=n==5U?-1:(int)sizeof cli_reply+1;
                        am13e_app_serial_cli_service_pending();break;
                    case 7:aux_fixture_initialized=0U;am13e_app_erpm_write(1);break;
                    case 8:hard_fault_handler();break; /* LED not initialized: must not recurse */
                    case 9:
                        test_primask=1U;initled();test_primask=0U;
                        hard_fault_handler();break;
                    case 10:am13e_app_motor_init();break; /* invalid IRQ-enabled initialization */
                    case 11:
                        adc_initialized=adc_inflight=1U;
                        ADC0->ADCINTFLG=ADC_ADCINTFLG_ADCINT1_MASK;
                        ADC0_INT1_IRQHandler();break;
                    case 12:(void)fixture_returned_main();break;
                    case 13:test_primask=1U;am13e_app_motor_init();break; /* duplicate init */
                    case 14:
                        fixture_gpio_power_failure=1U;
                        am13e_app_motor_commutation_enable(0);break;
                    case 15:
                        DL_MCPWM_init(MCPWM4,NULL);break; /* actual SDK ASSERT(false) */
                    default:assert(0);
                }
                assert(0); /* Every injected fatal condition is nonreturning. */
            }
            assert(watchdog_arms==1U && test_primask==1U);
            assert(!(GPIO1->DOE31_0&AM13E_GATE_MASK) || !gate_on());
            if(!fixture_gpio_power_failure)
                assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
            assert(!DL_Timer_isRunning(TIMG12));
            assert(!(ECAP1->ECEINT&(DL_ECAP_ISR_SOURCE_CEVT1|DL_ECAP_ISR_SOURCE_CTROVF)));
            /* Fault remains latched; an attempted restart also fails closed. */
            if(!setjmp(fault_return)){am13e_app_motor_commutation_enable(1);assert(0);}
            assert(watchdog_arms==2U);
            _exit(0);
        }
        int status;assert(waitpid(child,&status,0)==child);
        assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    printf("PASS production fatal shutdown: %u %s injections, latched restart refusal and reset ordering\n",
        count,early?"pre-initialization":"active-output");
}
#endif
#ifdef HALL_MAP
static void hall_input(unsigned code)
{
    *(volatile uint32_t *)(uintptr_t)&GPIO0->DIN31_0=
        (GPIO0->DIN31_0&AM13E_NFAULT_MASK)|
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
    DL_GPIO_initDigitalInput(AM13E_NFAULT_PINCM);
    *(volatile uint32_t *)(uintptr_t)&AM13E_NFAULT_GPIO->DIN31_0=AM13E_NFAULT_MASK;
    assert(!setjmp(fault_return));
#ifdef AM13E_FATAL_PATH_FIXTURE
    fatal_paths(1);
    am13e_app_aux_gpio_init();
#endif
    am13e_app_motor_init();
    assert((MCPWM4->TBCTL&MCPWM_TBCTL_CLKDIV_MASK)==
        (uint32_t)DL_MCPWM_CLOCK_DIVIDER_2<<MCPWM_TBCTL_CLKDIV_OFS);
    assert(am13e_app_motor_inactive_aq_boot_preflight());
    assert(INPUTXBAR->INPUTSELECT[DL_XBAR_INPUT2]==22U);
    assert(PWMXBAR->PWM_XBAR_GXSEL[DL_XBAR_TRIP2].PWMXBARG0SEL==(1U<<21U));
    assert(PWMXBAR->PWM_XBAR_GXSEL[DL_XBAR_TRIP3].PWMXBARG0SEL==0x54U);
    assert((MCPWM4->TZSEL&(DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3))==
           (DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3));
    CMPSS0->DACLVALS=333U; /* Stale unused low-DAC state from a prior owner. */
    am13e_app_motor_bemf_init();
#ifdef AM13E_SC5_FIXTURE
    sc5_adc();assert(munmap(r,0x07000000)==0);return 0;
#endif
    assert(CMPSS0->DACLVALS==0U);
    assert(!am13e_app_motor_bemf_reference_ready());
    assert(!am13e_motor_output_start() && !gate_on());
    assert(!am13e_app_motor_bemf_reference_set(4096U));
    assert(!am13e_app_motor_bemf_reference_set(0U));
    assert(am13e_app_motor_bemf_reference_set(2048U));
    assert(am13e_app_motor_bemf_reference_ready());
    calibration_lifecycle();
#ifdef PWM_ENABLE
    assert((MCPWM4->TZCTL&MCPWM_TZCTL_TZB_MASK)==
        ((uint32_t)DL_MCPWM_TZ_ACTION_HIGH<<MCPWM_TZCTL_TZB_OFS));
    checkcfg();assert(cfg.damp==1);
#endif
    for(unsigned n=0;n<18U;++n){ECAP1->TSCTR+=12500U;am13e_app_motor_bemf_tick();}
    assert(am13e_app_motor_inactive_preflight_ok());
    assert(CMPSS0->DACHVALS==2048U);
    assert((CMPSS0->COMPCTL&CMPSS_LITE_COMPCTL_COMPHSOURCE_MASK)==DL_CMPSSLITE_INSRC_DAC);
    for(unsigned phase=1U;phase<=3U;++phase){
        for(unsigned invert=0U;invert<2U;++invert){
            ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;
            compctl((int)(phase|(invert<<2U)));
            assert(DL_ECAP_getInterruptSource(ECAP1)==0U);
            assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
            assert((SYSCTL->SOCLOCK.CMPHPMXSEL&SYSCTL_CMPHPMXSEL_CMP0HPMXSEL_MASK)==phase-1U);
            assert((ECAP1->ECCTL0&ECAP_ECCTL0_INPUTSEL_MASK)==DL_ECAP_INPUT_CMPSS0_CTRIPH);
            assert(!am13e_app_motor_bemf_reference_set(1024U));
        }
    }
    compctl(0);
#if AM13E_OPTION1_OUTPUTS_QUALIFIED && !defined(HALL_MAP)
    /* Real laststep() stages a lock image, then disables sensing. A later
     * ZERO must not resurrect that cancelled comparator association. */
    for(unsigned hold=0U;hold<=2U;++hold){
        sine=prep=0;step=2;lock=(char)hold;reverse=0;
        ertm=18000;ival=3000;sync=0;
        laststep();
        assert(!am13e_app_motor_bemf_publication_pending());
        zero();assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    }
    lock=0;
#endif
#if !AM13E_OPTION1_OUTPUTS_QUALIFIED
    assert(!am13e_motor_output_start() && !am13e_power_stage_enable());
    assert(am13e_app_motor_inactive_preflight_ok() && !gate_on());
    puts("PASS unqualified Option 1 refuses PWM pad connection and Gate Enable");
    assert(munmap(r,0x07000000)==0);return 0;
#endif
#ifdef HALL_MAP
    hall_runtime();assert(munmap(r,0x07000000)==0);return 0;
#endif
#ifndef HALL_MAP
    timing_epochs();
#endif
    cfg.damp=1;cfg.sine_power=8;ertm=6000;ival=3000;sync=6;step=0;reverse=0;
    am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);
    nextstep();start();carrier();
#ifdef AM13E_FATAL_PATH_FIXTURE
    const uint32_t saved_mask=test_primask;test_primask=1U;
    am13e_app_adc_init();test_primask=saved_mask;
    AM13E_AdcRaw raw;
    assert(!am13e_app_adc_get_raw(&raw));
    ADC_Regs *adcs[]={ADC0,ADC1,ADC2};
    ADC_RESULT_Regs *results[]={ADC0RESULT,ADC1RESULT,ADC2RESULT};
    const unsigned channels[3][3]={{5U,12U,4U},{4U,12U,2U},{12U,5U,19U}};
    for(unsigned i=0;i<3U;++i){
        const uint32_t seq=*(volatile uint32_t *)((uintptr_t)adcs[i]+
            ADC_ADCSEQ1CONFIG+AM13E_ADC_SEQUENCE*DL_ADCSEQCONFIG_STEP);
        assert(!(seq&ADC_ADCSEQ1CONFIG_SAMPCAPRESETDISABLE_MASK));
        assert(!(seq&ADC_ADCSEQ1CONFIG_SAMPCAPRESETSEL_MASK));
        for(unsigned j=0;j<3U;++j){
            const uint32_t ctl=*(volatile uint32_t *)((uintptr_t)adcs[i]+ADC_ADCSOC0CTL+j*DL_ADCSOCCTL_STEP);
            assert(((ctl&ADC_ADCSOC0CTL_CHSEL_MASK)>>ADC_ADCSOC0CTL_CHSEL_OFS)==channels[i][j]);
            *(volatile uint16_t *)((uintptr_t)results[i]+ADC_ADCRESULT0+j*DL_ADCRESULT_STEP)=(uint16_t)(i*3U+j+1U);
        }
        adcs[i]->ADCINTFLG=ADC_ADCINTFLG_ADCINT1_MASK|ADC_ADCINTFLG_ADCINT1RESULT_MASK;
    }
    tick=UINT32_MAX-3U;adctrig();adctrig();
    ADC2_INT1_IRQHandler();assert(!am13e_app_adc_get_raw(&raw));
    ADC0_INT1_IRQHandler();assert(!am13e_app_adc_get_raw(&raw));
    tick=2U;ADC1_INT1_IRQHandler();assert(am13e_app_adc_get_raw(&raw));
    assert(raw.current_v==1U && raw.phase_a==2U && raw.ibus==3U &&
           raw.current_w==4U && raw.vbus==5U && raw.phase_b==6U &&
           raw.current_u==7U && raw.phase_c==8U && raw.ntc==9U &&
           raw.sample_count==1U && raw.trigger_overruns==1U);
    assert(raw.acquisition_tick==UINT32_MAX-3U && raw.completion_tick==2U);
    adc_latest.sample_count=UINT32_MAX;adc_sample_seq=UINT32_MAX-1U;
    adctrig();ADC0_INT1_IRQHandler();ADC1_INT1_IRQHandler();ADC2_INT1_IRQHandler();
    assert(am13e_app_adc_get_raw(&raw) && raw.sample_count==0U && adc_sample_seq==0U);
    adc_sample_seq=1U;assert(!am13e_app_adc_get_raw(&raw));adc_sample_seq=0U;
    puts("PASS actual Option 1 ADC0/1/2 SOC map, ordered completion and coherent nine-code snapshot");
    fatal_paths(0);
#endif
    assert(MCPWM4->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS &&
        MCPWM4->DBFED==(AM13E_MOTOR_OUTPUT_FED_TICKS?AM13E_MOTOR_OUTPUT_FED_TICKS:1U));
    inject_shadow_zero=1U;
    for(unsigned n=0;n<12U;++n){step_publication();assert(am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));}
    reverse=1;for(unsigned n=0;n<12U;++n){step_publication();}
#ifdef SW_BLANKING
    /* Real ZERO/CMPC IRQ -> ECAP capture gating, elapsed counter intact.
     * Coalesced old-CMPC/new-ZERO events must leave capture blanked. */
    const uint32_t elapsed=ECAP1->TSCTR;
    zero();assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    const int before_blank=ival;
    ECAP1->CAP1=500000U;ECAP1->ECFLG=DL_ECAP_ISR_SOURCE_CEVT1;
    ECAP1_IRQHandler();assert(ival==before_blank);
    MCPWM4->TBCTR=10U;MCPWM4->INTFLAG=DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2;
    MCPWM4_IRQHandler();assert(!(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK));
    MCPWM4->TBCTR=AM13E_BEMF_BLANK_TICKS;MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;
    MCPWM4_IRQHandler();assert(ECAP1->ECCTL1&ECAP_ECCTL1_CAPLDEN_MASK);
    assert(ECAP1->TSCTR==elapsed);
#endif
    /* Empty vectors after replacement must preserve the new hardware owner. */
    const int before_empty=step;
    MCPWM4->INTFLAG=0U;MCPWM4_IRQHandler();assert(gate_on());
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
    MCPWM4->INTFLAG=DL_MCPWM_INT_ET_1;MCPWM4_IRQHandler();
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
        assert((MCPWM4->TBCTL&MCPWM_TBCTL_CLKDIV_MASK)==
            (uint32_t)(mode==1?DL_MCPWM_CLOCK_DIVIDER_32:DL_MCPWM_CLOCK_DIVIDER_2)<<MCPWM_TBCTL_CLKDIV_OFS);
        assert(MCPWM4->DBRED==(mode==1?(AM13E_MOTOR_OUTPUT_RED_TICKS+15U)/16U:AM13E_MOTOR_OUTPUT_RED_TICKS));
#ifdef PWM_ENABLE
        assert(MCPWM4->DBFED==MCPWM4->DBRED);
#endif
        am13e_app_motor_audio_period(2000U);
        am13e_app_motor_audio_compare(400U,700U);zero();zero();carrier();assert(gate_on());
        am13e_app_motor_audio_compare(300U,500U);zero();carrier();
        am13e_app_motor_audio_period(1800U);assert(!gate_on());
        am13e_app_motor_audio_compare(0U,400U);zero();zero();carrier();
        am13e_app_motor_audio_end();assert(!gate_on());
        assert(am13e_app_motor_inactive_preflight_ok());
        assert(MCPWM4->DBRED==AM13E_MOTOR_OUTPUT_RED_TICKS);
    }
    /* Actual music adapter: constant-tone CCR>ARR and sticky UIF behavior
     * after a missed period, even when counter order does not show wrap. */
    am13e_app_audio_music_begin();zero();zero();
    am13e_app_audio_music_note(0,10,255);zero();zero();carrier();
    assert(MCPWM4->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    assert(MCPWM4->PWM1_AQCTLB==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
    assert((MCPWM4->ETSEL&MCPWM_ETSEL_ET2_SEL_MASK)==
        (uint32_t)DL_MCPWM_EVT_TBCTR_ZERO<<MCPWM_ETSEL_ET2_SEL_OFS);
    MCPWM4->TBCTR=5U;MCPWM4->INTFLAG|=DL_MCPWM_INT_ET_2;
    MCPWM4_IRQHandler();
    assert(!(MCPWM4->INTEN&DL_MCPWM_INT_ET_2));
    assert(MCPWM4->INTFLAG&DL_MCPWM_INT_ET_2);
    MCPWM4->ETCNT=1U<<MCPWM_ETCNT_ET2_CNT_OFS; /* queued extra old period */
    am13e_app_audio_music_tick();
    assert(DL_MCPWM_getEventTriggerEventCount(MCPWM4,DL_MCPWM_ET_2)==0U);
    zero();carrier();
    assert(MCPWM4->PWM3_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    assert(MCPWM4->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
    assert(!(MCPWM4->INTFLAG&DL_MCPWM_INT_ET_2));
    am13e_app_audio_music_tick(); /* no new ZERO: no second swap */
    zero();carrier();assert(MCPWM4->PWM3_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
    am13e_app_audio_music_pause();zero();carrier();
    am13e_app_audio_end();assert(!gate_on());
    /* Maximum 16-bit carrier image: inactive/freewheel compares must not
     * overflow merely because the electrical DT offset is added. */
    {
        const uint16_t image[6]={DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA,
            DL_MCPWM_AQ_OUTPUT_LOW_ZERO|DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB,0,DL_MCPWM_AQ_OUTPUT_HIGH_ZERO,0,0};
        const uint16_t maximum[6]={65535,65535,65535,65535,65535,65535};
        am13e_motor_output_stage(image,maximum,65534U);
        assert(MCPWM4->PWM1_AQCTLA==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO);
        assert(MCPWM4->PWM1_AQCTLB==DL_MCPWM_AQ_OUTPUT_LOW_ZERO);
        assert(MCPWM4->PWM1_CMPA==1U && MCPWM4->PWM1_CMPB==1U);
        assert(!gate_on());
    }
    /* A trip can latch halfway through masked SHADOW writes. Test in
     * an isolated anonymous-RAM child because fault reset never returns. */
    for(volatile unsigned fault_case=0;fault_case<3U;++fault_case){
    pid_t child=fork();assert(child>=0);
    if(child==0){
        sine=0;step=0;lock=0;nextstep();
        am13e_app_motor_pwm_apply(800,24,48,ertm,1,0,0,1);start();carrier();
        test_primask=0U;
        if(fault_case==1U){
            am13e_app_motor_commutation_enable(0);
            am13e_app_audio_music_begin();zero();zero();
            am13e_app_audio_music_note(0,0,50);zero();zero();carrier();
        }
        if(!setjmp(fault_return)){
            if(fault_case==1U){
                inject_ack_trip=1U;MCPWM4->INTFLAG=DL_MCPWM_INT_ET_2;
            }else if(fault_case==2U){
                MCPWM4->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ3;
                MCPWM4->INTFLAG=DL_MCPWM_INT_TZ_OST;
            }else{
                inject_trip=1U;
                am13e_app_motor_pwm_apply(1000,24,48,ertm,1,0,0,1);
                assert(test_primask==0U && (MCPWM4->TZCBCOSTFLAG&DL_MCPWM_TZ_FLAG_OST_TZ2));
            }
            MCPWM4_IRQHandler();assert(0);
        }
        assert(!gate_on() && watchdog_arms==1U);
        assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
        assert(!DL_MCPWM_getGlobalLoadOneShotLatchStatus(MCPWM4));
        _exit(0);
    }
    int child_status;assert(waitpid(child,&child_status,0)==child);
    assert(WIFEXITED(child_status) && WEXITSTATUS(child_status)==0);
    }
    /* Fault dominates a pending startup ZERO and cannot re-enable pads. */
    sine=0;step=0;lock=0;nextstep();am13e_app_motor_commutation_commit();
    am13e_app_motor_commutation_enable(1);
    if(!setjmp(fault_return)){
        MCPWM4->TZCBCOSTFLAG=DL_MCPWM_TZ_FLAG_OST_TZ2;
        MCPWM4->INTFLAG=DL_MCPWM_INT_TZ_OST|DL_MCPWM_INT_ET_1;
        MCPWM4_IRQHandler();assert(0);
    }
    assert(watchdog_arms==1U && !gate_on());
    assert(am13e_mcu_motor_pads_disconnected(am13e_board_motor_pad_route()));
    assert(MCPWM4->TZCBCOSTFLAG&DL_MCPWM_TZ_FLAG_OST_TZ2);
    if(!setjmp(fault_return)){am13e_app_motor_commutation_enable(1);assert(0);}
    assert(!gate_on());
    assert(!(MCPWM4->INTEN&DL_MCPWM_INT_ET_1));
    assert(munmap(r,0x07000000)==0);
    puts("PASS original Core -> native MCPWM/pinmux/gate, sine/sixstep/reverse/brake and dominant OST shutdown");
}
