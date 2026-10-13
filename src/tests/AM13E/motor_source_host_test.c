/* Actual source laststep/commutation/BEMF functions + util resetcom and
 * native BEMF/phase planners. Physical PWM/comparator/timer/reset mocked. */
#include "../../main.c"
#include "motor_phase_plan.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <limits.h>
TestSCB test_scb;
uint32_t test_primask;
static char trace[64];
static unsigned trace_length,stopping,scheduled,shutdowns;
static int scheduled_delay,selected_comparator,filter_ertm,held_step;
static jmp_buf fault_return;
static AM13E_SixstepPlan phase;
static void event(char value)
{
    if(stopping)assert(test_primask==1U);
    assert(trace_length<sizeof(trace)-1U);
    trace[trace_length++]=value;trace[trace_length]=0;
}
static void clear_trace(void) { trace_length=0U;trace[0]=0; }
void test_irq_disable(void) { test_primask=1U;event('d'); }
void test_irq_restore(uint32_t mask)
{ assert(test_primask==1U);stopping=0U;test_primask=mask;event('m'); }
void am13e_app_commutation_reset(void) { event('R'); }
void am13e_app_motor_drag_brake_write(void) { event('D'); }
void am13e_app_motor_commutation_commit(void) { event('K'); }
void compctl(int code) { selected_comparator=code;event('C'); }
void am13e_app_motor_sixstep_write(int p,int n,int cc,int damp,int rev)
{ assert(am13e_motor_plan_sixstep(p,n,cc,damp,rev,&phase));event('S'); }
void am13e_app_motor_bemf_interval_select(int value) { filter_ertm=value;event('F'); }
void am13e_app_motor_lock_brake_stage(int value,int phase_step)
{ assert(value==1 || value==2);held_step=phase_step;event('L'); }
void am13e_app_motor_sine_schedule_us(int value) { (void)value;event('T'); }
void am13e_app_motor_sine_write(int a,int b,int c,int power,int start)
{ (void)a;(void)b;(void)c;(void)power;(void)start;event('W'); }
void am13e_app_motor_sine_finish(void) { event('E'); }
void am13e_app_motor_bemf_commutation_delay_us(int value)
{ ++scheduled;scheduled_delay=value;event('Q'); }
void am13e_app_motor_fault_shutdown(void) { ++shutdowns;event('O'); }
__attribute__((noreturn)) void am13e_app_motor_fault_reset(void)
{ event('X');assert(shutdowns==1U);longjmp(fault_return,1); }
static void stop(unsigned mask,int hold,int sine_step)
{
    test_primask=mask;stopping=1U;clear_trace();
    sine=sine_step?100:0;prep=sine_step?1:0;step=sine_step?sine_step:2;
    lock=(char)hold;reverse=0;throt=0;ertm=0;ival=2000;sync=0;
    laststep();
    assert(test_primask==mask && !step && !sine && !prep && selected_comparator==0);
    if(hold)assert(!strcmp(trace,"dRSFLKCm") && held_step==oldstep);
    else assert(!strcmp(trace,"dRDKCm"));
    if(sine_step==359)assert(oldstep==(hold?2:1)); /* original sine-to-sixstep rounding/wrap */
}
static void bemf(void)
{
    clear_trace();scheduled=0U;sync=2;fast=1;ival=3000;ertm=1000;cfg.timing=16;
    assert(!am13e_app_motor_on_bemf_event(1499,0));
    assert(sync==2 && fast==1 && ival==3000 && !scheduled && !trace_length);
    assert(am13e_app_motor_on_bemf_event(1500,0));
    assert(sync==3 && fast==1 && ival==2625 && scheduled==1U && scheduled_delay==656);
    clear_trace();sync=6;ival=3000;ertm=3000;
    assert(am13e_app_motor_on_bemf_event(3000,0));
    assert(sync==6 && !fast && ival==3000 && scheduled_delay==750);
    clear_trace();ival=3000;ertm=1000;
    assert(am13e_app_motor_on_bemf_event(4501,0));
    assert(fast && ival==3375 && scheduled_delay==844);
    const unsigned before=scheduled;clear_trace();
    assert(am13e_app_motor_on_bemf_event(0,1));
    assert(!sync && !fast && ival==10000 && ertm==100000000 && scheduled==before && !trace_length);
    /* Invalid native timing must use real source fault ordering, never
     * produce a fabricated timer delay or modify accepted source state. */
    ival=INT_MAX;sync=4;fast=1;shutdowns=0U;clear_trace();
    if(!setjmp(fault_return)){(void)am13e_app_motor_on_bemf_event(1000,0);assert(0);}
    assert(!strcmp(trace,"OX") && scheduled==before && sync==4 && fast==1);
}
int main(void)
{
    memcpy(&cfg,&cfgdata,sizeof cfg);cfg.damp=1;cfg.throt_ztc=0;
    stop(0U,0,0);stop(1U,0,359);
    stop(0U,1,0);stop(1U,2,359);
    clear_trace();test_primask=0;step=0;sync=0;sine=0;prep=0;reverse=0;ertm=3000;ival=2000;
    am13e_app_motor_on_commutation_event();
    assert(step==1 && !strcmp(trace,"SF") && filter_ertm==3000);
    /* Source desync detector: establish old interval then six fast
     * commutations with >50% interval change, without copying its state. */
    ival=2000;am13e_app_motor_on_commutation_event();
    ival=100;sync=6;fast=1;ertm=1000;
    for(unsigned n=0U;n<6U;++n){clear_trace();am13e_app_motor_on_commutation_event();}
    assert(!sync && !fast && ival==5000 && ertm==100000000);
    bemf();
    puts("PASS production motor laststep IRQ preservation, drag/lock, desync and BEMF fault/timeout policy");
    return 0;
}
