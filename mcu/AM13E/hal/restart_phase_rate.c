#include "restart_phase_rate.h"
#ifdef AM13E_SIM_ONLY_RESTART_PHASE_RATE
#if !defined(AM13E_SIM_ONLY_BOARD_PROFILE)
#error "Restart phase-rate experiment requires SIM_ONLY"
#endif
#include <stdint.h>
static uint32_t last_sample,last_tick,last_epoch,run,have;
static int last_direction;
static int32_t last_angle;
static volatile uint32_t published_tick,published_epoch;
static volatile int published_period,published_direction;
/* Integer CORDIC atan2 in radians*2^20. Generated constants are atan(2^-i),
 * not motor measurements. Avoids the disabled FP unit in this target. */
static int32_t vector_angle(int32_t a,int32_t b,int32_t c)
{
    static const int32_t turns[]={823550,486170,256879,130396,65451,32757,16383,8192,4096,2048,1024,512,256,128,64,32,16,8,4,2};
    int64_t x=(int64_t)(2*a-b-c)*65536;
    int64_t y=(int64_t)(b-c)*113512; /* sqrt(3)*65536 */
    int32_t angle=0;
    if(x<0){angle=y>=0?3294199:-3294199;x=-x;y=-y;}
    for(unsigned i=0;i<20U;++i){
        const int direction=y>0?1:-1;
        const int64_t next_x=x+direction*(y>>i);
        y=y-direction*(x>>i);x=next_x;
        angle+=direction*turns[i];
    }
    return angle;
}
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
    const int32_t angle=vector_angle((int32_t)a,(int32_t)b,(int32_t)c);
    if(have && epoch==last_epoch && (uint32_t)(r->sample_count-last_sample)==1U){
        const uint32_t dt=r->acquisition_tick-last_tick;
        int32_t delta=angle-last_angle;
        if(delta>3294199)delta-=6588397;
        if(delta< -3294199)delta+=6588397;
        const int direction=delta>0?1:-1;
        const uint32_t magnitude=(uint32_t)(delta>0?delta:-delta);
        if(dt>0U && dt<=32U && magnitude>=20972U && magnitude<1647099U){
            if(run && last_direction!=direction)run=0U;
            ++run;last_direction=published_direction=direction;
            const uint64_t divisor=2U*(uint64_t)magnitude;
            const uint64_t us=(UINT64_C(1098066)*dt*125U+divisor/2U)/divisor;
            if(run>=2U && us>=1U && us<=32768U){
                published_tick=r->acquisition_tick;published_epoch=epoch;
                published_period=(int)us;
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
#else
typedef int restart_phase_rate_not_selected;
#endif
