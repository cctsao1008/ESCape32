/* UNIT vectors only; never written into a native QEMU run. */
#include "restart_phase_rate.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
static AM13E_AdcRaw raw(unsigned sample,uint32_t t,float angle)
{
    AM13E_AdcRaw r={0};r.sample_count=sample;r.acquisition_tick=r.completion_tick=t;
    r.phase_a=(uint16_t)lroundf(2000.0f+100.0f*sinf(angle));
    r.phase_b=(uint16_t)lroundf(2000.0f+100.0f*sinf(angle-2.094395102393195f));
    r.phase_c=(uint16_t)lroundf(2000.0f+100.0f*sinf(angle+2.094395102393195f));return r;
}
static int feed(float sign,uint32_t start,uint32_t epoch)
{
    am13e_restart_rate_invalidate();
    for(unsigned n=0;n<3;++n){AM13E_AdcRaw r=raw(n+1,start+n*16U,sign*(float)n*.2617993878f);am13e_restart_rate_update(&r,epoch);}
    return am13e_restart_rate_period(start+32U,epoch,sign<0);
}
int main(void)
{
    am13e_restart_rate_invalidate();assert(!am13e_restart_rate_period(0,0,0));
    int p=feed(1,0,3);assert(p>3900 && p<4100);
    assert(!am13e_restart_rate_period(32,3,1));assert(!am13e_restart_rate_period(49,3,0));assert(!am13e_restart_rate_period(32,4,0));
    assert(feed(-1,0,4)>3900);assert(!am13e_restart_rate_period(32,4,0));
    assert(feed(1,UINT32_MAX-31U,5)>3900);
    AM13E_AdcRaw r=raw(4,48,.7853981634f);r.phase_a=2000;r.phase_b=r.phase_c=2001;am13e_restart_rate_update(&r,5);assert(!am13e_restart_rate_period(48,5,0));
    (void)feed(1,0,6);r=raw(5,48,.7853981634f);am13e_restart_rate_update(&r,6);assert(!am13e_restart_rate_period(48,6,0));
    (void)feed(1,0,6);r=raw(4,48,.7853981634f);r.completion_tick=81;am13e_restart_rate_update(&r,6);assert(!am13e_restart_rate_period(48,6,0));
    (void)feed(1,0,6);r=raw(4,48,.7853981634f);r.phase_c=4095;am13e_restart_rate_update(&r,6);assert(!am13e_restart_rate_period(48,6,0));
    (void)feed(1,0,6);am13e_restart_rate_update(NULL,6);assert(!am13e_restart_rate_period(32,6,0));
    (void)feed(1,0,6);r=raw(4,48,.7853981634f);am13e_restart_rate_update(&r,7);assert(!am13e_restart_rate_period(48,7,0));
    (void)feed(1,0,6);r=raw(4,48,2.9f);am13e_restart_rate_update(&r,6);assert(!am13e_restart_rate_period(48,6,0));
    /* Observable counterexample: one extra full rotation per ADC period
     * has identical samples. Do not claim this estimates physical direction
     * without an independently established bandwidth bound. */
    for(unsigned n=0;n<3;++n){AM13E_AdcRaw a=raw(n+1,n*16U,n*.2617993878f),b=raw(n+1,n*16U,n*(.2617993878f+6.2831853072f));assert(a.phase_a==b.phase_a && a.phase_b==b.phase_b && a.phase_c==b.phase_c);}
    puts("PASS_NEW phase-rate coherent/fresh/epoch/direction/weak/rail/gap/wrap negatives; INCOMPLETE standalone physical direction due explicit alias counterexample");
    return 0;
}
