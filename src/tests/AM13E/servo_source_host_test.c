/* Real decoder -> shared Rel17 io.c callback -> shared util.c scale().
 * Only watchdog/system-reset hardware boundaries are replaced. */
#include "common.h"
#include "io_backend.h"
#include "command_decode.h"
#include "servo_calibration.h"
#include "receiver_entry.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

Cfg cfg;
int throt, ertm;
char telreq;
static unsigned feeds, resets;
static jmp_buf reset_return;
void am13e_app_io_watchdog_feed(void) { ++feeds; }
__attribute__((noreturn)) void am13e_app_service_reset(void)
{ ++resets; longjmp(reset_return, 1); }
static void pulses(AM13E_PB14_Decoder *d,unsigned width,unsigned period)
{
    const uint32_t start=d->active?d->last_start+period*200U:100000U;
    am13e_pb14_decoder_pulse(d,start,start+width*200U,
                               am13e_app_io_servo_pulse,NULL);
}
static void calibration(void)
{
    const unsigned nominal[]={22000,20000,11000,10000,8000,5000,4000,3000,2000};
    const unsigned low[]={21120,19200,10560,9600,7680,4800,3840,2880,1920};
    const unsigned high[]={22880,20800,11440,10400,8320,5200,4160,3120,2080};
    for(unsigned i=0;i<9U;++i){
        AM13E_ServoCalibration s={0};
        for(unsigned n=0;n<3U;++n)
            assert(am13e_servo_calibration_step(&s,nominal[i]+10U,1)==
                   AM13E_SERVO_CAL_WAIT);
        assert(am13e_servo_calibration_step(&s,nominal[i]+10U,1)==
               AM13E_SERVO_CAL_SLOW);
        assert(s.error==0 && s.low==-40);
        for(unsigned n=0;n<3U;++n)
            assert(am13e_servo_calibration_step(&s,nominal[i]-20U,1)==
                   AM13E_SERVO_CAL_WAIT);
        assert(am13e_servo_calibration_step(&s,nominal[i]-20U,1)==
               AM13E_SERVO_CAL_FAST);
        assert(s.high==80 && s.error==0);
        for(unsigned n=0;n<3U;++n)
            assert(am13e_servo_calibration_step(&s,nominal[i],1)==
                   AM13E_SERVO_CAL_WAIT);
        assert(am13e_servo_calibration_step(&s,nominal[i],1)==
               AM13E_SERVO_CAL_DONE && s.finished);
        s=(AM13E_ServoCalibration){0};
        assert(am13e_servo_calibration_step(&s,low[i],1)==AM13E_SERVO_CAL_DONE);
        s=(AM13E_ServoCalibration){0};
        assert(am13e_servo_calibration_step(&s,high[i],1)==AM13E_SERVO_CAL_DONE);
        s=(AM13E_ServoCalibration){0};
        assert(am13e_servo_calibration_step(&s,nominal[i],0)==AM13E_SERVO_CAL_DONE);
    }
    AM13E_ServoCalibration s={.count=255};
    assert(am13e_servo_calibration_step(&s,20010U,1)==AM13E_SERVO_CAL_SLOW);
    assert(s.count==0U);
}
int main(void)
{
    cfg.throt_min=1000; cfg.throt_mid=1500; cfg.throt_max=2000;
    AM13E_PB14_Decoder d;
    am13e_pb14_decoder_reset(&d,200000000U,0);
    for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES;++n)
        pulses(&d,1500,2500); /* Period >2000us selects Servo. */
    assert(d.rx_mode==AM13E_PB14_RX_PWM && feeds==1U && throt==947);
    for(unsigned width=28U;width<=32U;++width){
        pulses(&d,width,20000); pulses(&d,width,20000);
        assert(telreq==1 && throt==947);
        telreq=0;
    }
    const unsigned old=feeds;
    ertm=100; /* Invalid signal while running: ignore, no watchdog feed. */
    pulses(&d,799,20000); pulses(&d,799,20000);
    assert(feeds==old+1U && throt==947); /* first pending width was 32 */
    pulses(&d,2201,20000); pulses(&d,2201,20000);
    assert(feeds==old+1U && resets==0U && throt==947);
    ertm=0;
    if(!setjmp(reset_return)) {
        pulses(&d,2201,20000);
        assert(0); /* Original idle invalid-signal reset is nonreturning. */
    }
    assert(resets==1U && feeds==old+1U);
    am13e_pb14_decoder_reset(&d,200000000U,0);
    for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES;++n)
        pulses(&d,150,500);
    assert(d.rx_mode==AM13E_PB14_RX_ONESHOT && throt==315);
    /* 3.5us Oneshot capture is 28 logical counts, not rounded to 4us*8. */
    uint32_t start=d.last_start+100000U;
    am13e_pb14_decoder_pulse(&d,start,start+700U,am13e_app_io_servo_pulse,NULL);
    start+=100000U;
    am13e_pb14_decoder_pulse(&d,start,start+700U,am13e_app_io_servo_pulse,NULL);
    assert(telreq==1 && throt==315);
    cfg.throt_cal=1;
    am13e_app_io_servo_pulse(1500,20000);
    assert(am13e_app_io_servo_calibration_blocked()==1);
    cfg.input_mode=3;
    const unsigned before=feeds;
    am13e_app_io_servo_pulse(30,20000);
    assert(feeds==before);
    cfg.input_mode=0;cfg.throt_cal=0;
    /* Width must never choose the timer domain. A 150us pulse at
     * 20ms selects Servo and reaches original invalid-width handling,
     * rather than applying Oneshot throttle (150*8=1200). */
    am13e_pb14_decoder_reset(&d,200000000U,0);
    const unsigned rejected_feeds=feeds;
    throt=0;
    if(!setjmp(reset_return)) {
        for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES;++n)
            pulses(&d,150,20000);
        assert(0);
    }
    assert(resets==2U && feeds==rejected_feeds && throt==0);
    /* A transient valid pulse cannot lock discovery or feed WWDT. */
    am13e_pb14_decoder_reset(&d,200000000U,0);
    for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES-1U;++n)
        pulses(&d,1500,20000);
    assert(d.rx_mode==AM13E_PB14_RX_UNDECIDED && feeds==rejected_feeds);
    am13e_pb14_decoder_idle(&d,d.last_end+11000000U,NULL);
    for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES-1U;++n)
        pulses(&d,1500,20000);
    assert(d.rx_mode==AM13E_PB14_RX_UNDECIDED && feeds==rejected_feeds);
    pulses(&d,1500,20000);
    assert(d.rx_mode==AM13E_PB14_RX_PWM && feeds==rejected_feeds+1U);
    /* Source period boundary: 2000us takes the 8MHz domain. */
    am13e_pb14_decoder_reset(&d,200000000U,0);
    for(unsigned n=0;n<REL17_SERVO_ENTRY_CAPTURES;++n)
        pulses(&d,150,2000);
    assert(d.rx_mode==AM13E_PB14_RX_ONESHOT && throt==315);
    calibration();
    puts("PASS production Servo/Oneshot/telemetry/invalid-input source policy and calibration traces");
    return 0;
}
