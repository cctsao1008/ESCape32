#include "motor_timer_math.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
int main(void)
{
    assert(am13e_motor_us_to_timer_ticks(1,100000000)==100);
    assert(am13e_motor_us_to_timer_ticks(10,100000000)==1000);
    assert(am13e_motor_us_to_timer_ticks(100,100000000)==10000);
    assert(am13e_motor_us_to_timer_ticks(1000,100000000)==100000);
    assert(am13e_motor_us_to_timer_ticks(65535,100000000)==6553500);
    assert(am13e_motor_us_to_timer_ticks(0,100000000)==0);
    assert(am13e_motor_us_to_timer_ticks(123,0)==0);
    assert(am13e_motor_us_to_timer_ticks(UINT32_MAX,100000000)==0);
    assert(am13e_motor_us_to_timer_ticks(1,1)==0);
    puts("AM13E TIMG12 motor timer microsecond conversion PASS");
    return 0;
}
