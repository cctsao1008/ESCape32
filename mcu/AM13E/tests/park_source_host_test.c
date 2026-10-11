/* Include the actual Rel17 park() and state, not a copied state machine.
 * The native GPIO reader/writer are compiled alongside it. */
#include "../../../src/main.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <dl_gpio.h>
#include "motor_phase_plan.h"
GPIO_Regs test_gpio[3];
TestSCB test_scb;
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static jmp_buf halt_return;
void test_halt(void) { longjmp(halt_return,1); }
static AM13E_SixstepPlan phase;
static int comparator, selected_comparator, scheduled, sine_finished;
static unsigned phase_writes, events;
void am13e_app_motor_sixstep_write(int p,int n,int cc,int damp,int rev)
{
    assert(am13e_motor_plan_sixstep(p,n,cc,damp,rev,&phase));
    comparator=cc;
    ++phase_writes;
    assert(events++==0U);
}
void compctl(int cc) { selected_comparator=cc; assert(events++==1U); }
void am13e_app_motor_bemf_interval_select(int interval)
{ (void)interval; assert(events++==2U); }
void am13e_app_motor_sine_schedule_us(int interval) { scheduled=interval; }
void am13e_app_motor_sine_write(int a,int b,int c,int power,int start)
{
    assert(a>=0 && a<360 && b==(a<120?a+240:a-120) &&
           c==(a<240?a+120:a-240) && power==64);
    (void)start;
}
void am13e_app_motor_sine_finish(void) { ++sine_finished; }
static void motor_source(void)
{
    /* Execute actual shared nextstep(), retaining native phase planner.
     * Only physical motor/timer/comparator boundaries are replaced.
     */
    step=0;sine=0;prep=0;sync=6;ival=2000;ertm=6000;reverse=0;
    cfg.damp=1;cfg.throt_ztc=0;throt=1000;
    int old_cc=0;
    for(unsigned n=0;n<6U;++n){
        events=0U;nextstep();
        assert(step==(int)n+1 && events==3U && selected_comparator==old_cc);
        old_cc=comparator;
        assert(test_gpio[1].latch==(step>=4?2U:0U));
    }
    assert(phase_writes==6U && ertm==6000);
    reverse=1;step=1;
    for(unsigned n=0;n<6U;++n){
        events=0U;nextstep();
        assert(step==6-(int)n && phase.reverse && selected_comparator==old_cc);
        old_cc=comparator;
    }
    cfg.throt_ztc=1;throt=0;events=0;nextstep();
    for(unsigned n=0;n<3U;++n)assert(phase.phase[n]==AM13E_PHASE_FLOATING);
    sine=100;prep=1;step=0;reverse=0;cfg.sine_power=8;cutback=0;
    nextstep();assert(step==1 && scheduled==100 && test_gpio[1].latch==0U);
    step=180;nextstep();assert(step==181 && test_gpio[1].latch==2U);
    /* First sine sample finalizes entry and disables prior comparator. */
    prep=0;step=0;events=1U;nextstep();
    assert(sine_finished==1 && sync==0 && prep==1 && selected_comparator==0);
}
int main(void)
{
    am13e_app_aux_gpio_init();
    assert(test_gpio[1].power && (test_gpio[1].output&1U)==0U);
    assert(test_input[32] && test_function[33]==1U &&
           test_gpio[1].output==2U && test_gpio[1].latch==0U);
    am13e_app_erpm_write(1);
    assert(test_gpio[1].latch==2U);
    am13e_app_erpm_write(0);
    assert(test_gpio[1].latch==0U);
    park1=0;park2=0;park3=UINT16_MAX;reverse=0;
    for(unsigned n=0;n<800U;++n)assert(park());
    assert(park1==0 && !reverse);
    test_gpio[1].input=1U;
    assert(park() && park1==1 && park2==0U);
    test_gpio[1].input=0U;
    for(unsigned n=0;n<800U;++n)assert(park());
    assert(park1==2 && park3==UINT16_MAX);
    test_gpio[1].input=1U;
    for(unsigned n=0;n<800U;++n)assert(park());
    assert(park1==3 && reverse && park3==400U);
    for(unsigned n=0;n<399U;++n)assert(park());
    assert(!park());
    park1=0;park2=799;park3=UINT16_MAX;
    test_read_toggle=1U;
    assert(!park() && park1==0); /* Source 20-transition unstable refusal. */
    const unsigned before=test_enable_calls;
    if(!setjmp(halt_return)){am13e_app_aux_gpio_init();assert(0);}
    assert(test_enable_calls==before); /* repeated init cannot drive a new route */
    motor_source();
    puts("PASS production Rel17 park/ERPM, forward/reverse/coast/sine and delayed comparator order");
    return 0;
}
