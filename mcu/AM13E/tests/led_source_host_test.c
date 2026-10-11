/* Actual source PendSV status/DShot LED commands -> native TI GPIO backend.
 * Only peripheral DriverLib/CMSIS, ADC/telemetry and fault reset are mocked. */
#include "../../../src/main.c"
#include "io_backend.h"
#include <dl_gpio.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
GPIO_Regs test_gpio[3];
TestSCB test_scb;
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static jmp_buf halt_return;
static unsigned fault_off,feeds,adc_calls,telem_calls;
void test_halt(void) { longjmp(halt_return,1); }
void am13e_app_io_watchdog_feed(void) { ++feeds; }
void sendtelem(void) { ++telem_calls; }
void adctrig(void) { ++adc_calls; }
static unsigned physical(unsigned logical)
{
    unsigned inversion=0U;
#ifdef LED_INV
    inversion=7U;
#endif
#ifdef LED2_INV
    inversion|=2U;
#endif
    return (logical^inversion)&((1U<<LED_CNT)-1U);
}
void test_output_enable(GPIO_Regs *g,uint32_t mask)
{
    assert(g==GPIO1 && mask && !(mask&~((1U<<LED_CNT)-1U)));
    assert(g->latch==physical(0U)); /* every inactive latch BEFORE enable */
    for(unsigned i=0U;i<LED_CNT;++i)assert(test_function[32U+i]==1U);
}
void am13e_app_motor_fault_shutdown(void) { ++fault_off; }
__attribute__((noreturn)) void am13e_app_motor_fault_reset(void)
{ assert(fault_off==1U && test_gpio[1].latch==physical(1U));longjmp(halt_return,1); }
static void command(unsigned value)
{
    unsigned body=value*2U+1U;
    uint16_t frame=(uint16_t)((body<<4U)|((body^(body>>4U)^(body>>8U))&15U));
    assert(am13e_app_io_dshot_packet(frame,0));
}
int main(void)
{
    const unsigned before=test_enable_calls;
    if(!setjmp(halt_return)){ledctl(1);assert(0);}
    assert(test_enable_calls==before); /* no write before initialization */
    initled();
    assert(test_gpio[1].output==((1U<<LED_CNT)-1U) && test_gpio[1].latch==physical(0U));
    assert(test_function[32]==1U && test_function[33]==1U && test_function[34]==1U);
    for(unsigned value=0;value<16U;++value){ledctl((int)value);assert(test_gpio[1].latch==physical(value));}
    cfg.led=0;cfg.input_mode=0;tick=0;
    command(22U);pend_sv_handler();assert(cfg.led==1 && test_gpio[1].latch==physical(1U));
    command(23U);command(24U);
#if LED_CNT == 4
    command(25U);
#endif
    pend_sv_handler();assert(cfg.led==((1U<<LED_CNT)-1U) && test_gpio[1].latch==physical((1U<<LED_CNT)-1U));
    command(26U);command(27U);command(28U);
#if LED_CNT == 4
    command(29U);
#endif
    assert(!cfg.led);
    led=4;pend_sv_handler();assert(test_gpio[1].latch==physical(4U)); /* source status fallback */
    cfg.led=2;pend_sv_handler();assert(test_gpio[1].latch==physical(2U)); /* config overrides status */
    ertm=1;command(22U);assert(cfg.led==2);ertm=0;
    tick=1;cfg.led=3;const unsigned old_adc=adc_calls;pend_sv_handler();
    assert(adc_calls==old_adc && test_gpio[1].latch==physical(2U)); /* original 1kHz divider */
    if(!setjmp(halt_return)){hard_fault_handler();assert(0);}
    assert(fault_off==1U && test_gpio[1].latch==physical(1U));
    const unsigned enabled=test_enable_calls;
    if(!setjmp(halt_return)){initled();assert(0);}
    assert(test_enable_calls==enabled && feeds==(LED_CNT==4?9U:7U) && telem_calls==5U);
    puts("PASS production discrete LED GPIO, DShot commands, PendSV status and fault-off ordering");
    return 0;
}
