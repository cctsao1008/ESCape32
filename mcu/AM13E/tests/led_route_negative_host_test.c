#include "common.h"
#include <dl_gpio.h>
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>
GPIO_Regs test_gpio[3];
TestSCB test_scb;
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static jmp_buf stopped;
void test_halt(void) { longjmp(stopped,1); }
int main(void)
{
    if(!setjmp(stopped)){initled();assert(0);}
    assert(test_enable_calls==0U && test_gpio[0].output==0U &&
           test_gpio[1].output==0U && test_gpio[2].output==0U);
    puts("PASS invalid LED GPIO tuple/ownership refuses before output enable");
    return 0;
}
