/* Selected calibration executes shared io.c with a mocked trim actuator.
 * No AM13E hardware trim provider or physical qualification is asserted. */
#include "common.h"
#include "io_backend.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
Cfg cfg;
int throt, ertm;
char telreq;
static unsigned feeds, trims;
void am13e_app_io_watchdog_feed(void) { ++feeds; }
void am13e_app_servo_clock_trim(int direction)
{ assert(direction==-1); ++trims; }
__attribute__((noreturn)) void am13e_app_service_reset(void) { abort(); }
int main(void)
{
    cfg.throt_min=1000; cfg.throt_mid=1500; cfg.throt_max=2000;
    cfg.throt_cal=1;
    for(unsigned n=0;n<4U;++n)am13e_app_io_servo_pulse(1500,20010);
    assert(feeds==4U && trims==1U && throt==0);
    for(unsigned n=0;n<4U;++n)am13e_app_io_servo_pulse(1500,20000);
    /* Original calibirq feeds on the final eligible capture before switching. */
    assert(feeds==8U && trims==1U && throt==0);
    am13e_app_io_servo_pulse(1500,20000);
    assert(feeds==9U && trims==1U && throt==947);
    assert(am13e_app_io_servo_calibration_blocked()==0);
    puts("PASS production selected calibration feed/trim/Servo transition with mocked actuator");
    return 0;
}
