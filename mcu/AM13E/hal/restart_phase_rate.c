#include "restart_phase_rate.h"
#ifdef AM13E_SIM_ONLY_RESTART_PHASE_RATE
#if !defined(AM13E_SIM_ONLY_BOARD_PROFILE)
#error "Restart phase-rate experiment requires SIM_ONLY"
#endif
#include <math.h>
#include <stdint.h>
static uint32_t last_sample,last_tick,last_epoch,run,have;
static int last_direction;
static float last_angle;
static volatile uint32_t published_tick,published_epoch;
static volatile int published_period,published_direction;
static void unavailable(void){published_period=0;published_direction=0;}
void am13e_restart_rate_invalidate(void){unavailable();have=run=0U;}
void am13e_restart_rate_update(const AM13E_AdcRaw *r,uint32_t epoch)
{
    if(!r){am13e_restart_rate_invalidate();return;}
    if(have && r->sample_count==last_sample)return;
    unavailable();
    const unsigned a=r->phase_a,b=r->phase_b,c=r->phase_c;
    unsigned hi=a>b?a:b;hi=hi>c?hi:c;
    unsigned lo=a<b?a:b;lo=lo<c?lo:c;
    if(a==0U || b==0U || c==0U || hi>=4095U || hi-lo<8U ||
       (uint32_t)(r->completion_tick-r->acquisition_tick)>32U){am13e_restart_rate_invalidate();return;}
    const float angle=atan2f(1.7320508075688772f*((float)b-(float)c),
                            2.0f*(float)a-(float)b-(float)c);
    if(have && epoch==last_epoch && (uint32_t)(r->sample_count-last_sample)==1U){
        const uint32_t dt=r->acquisition_tick-last_tick;
        float delta=angle-last_angle;
        if(delta>3.141592653589793f)delta-=6.283185307179586f;
        if(delta< -3.141592653589793f)delta+=6.283185307179586f;
        const int direction=delta>0.0f?1:-1;
        if(dt>0U && dt<=32U && fabsf(delta)>=0.02f && fabsf(delta)<1.5707963267948966f){
            if(run && last_direction!=direction)run=0U;
            ++run;last_direction=published_direction=direction;
            const float us=1.0471975511965976f*(float)dt*62.5f/fabsf(delta);
            if(run>=2U && isfinite(us) && us>=1.0f && us<=32768.0f){
                published_tick=r->acquisition_tick;published_epoch=epoch;
                published_period=(int)(us+0.5f);
            }
        }else run=0U;
    }else run=0U;
    last_sample=r->sample_count;last_tick=r->acquisition_tick;
    last_epoch=epoch;last_angle=angle;have=1U;
}
int am13e_restart_rate_period(uint32_t now,uint32_t epoch,int reverse)
{
    if(!published_period || published_epoch!=epoch ||
       (uint32_t)(now-published_tick)>16U ||
       published_direction!=(reverse?-1:1))return 0;
    return published_period;
}
#endif
