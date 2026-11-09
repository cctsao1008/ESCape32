#include "coast_phase_plan.h"
#ifdef AM13E_SIM_ONLY_COAST_PHASE_ACQUIRE
#ifndef AM13E_SIM_ONLY_BOARD_PROFILE
#error "Coast phase acquisition requires explicit SIM_ONLY"
#endif
#include "commutation.h"
#include <string.h>
static AM13E_CoastPhase state;
static unsigned have,last_index,current_valid;
static uint32_t last_completion;
void am13e_coast_phase_reset(void){memset(&state,0,sizeof state);have=current_valid=0U;}
void am13e_coast_phase_update(const AM13E_AdcRaw *r,uint32_t epoch)
{
    static const unsigned forward_patterns[]={5,1,3,2,6,4};
    if(!r){am13e_coast_phase_reset();return;}
    if(have && state.sample==r->sample_count && state.epoch==epoch){
        /* A sample identity is immutable. Do not silently retain an older
         * authorization when its producer replays contradictory evidence. */
        if(state.tick!=r->acquisition_tick || last_completion!=r->completion_tick ||
           state.phase[0]!=r->phase_a || state.phase[1]!=r->phase_b ||
           state.phase[2]!=r->phase_c)am13e_coast_phase_reset();
        return;
    }
    if(have && (state.epoch!=epoch || (uint32_t)(r->sample_count-state.sample)!=1U ||
       (uint32_t)(r->acquisition_tick-state.tick)==0U ||
       (uint32_t)(r->acquisition_tick-state.tick)>32U))am13e_coast_phase_reset();
    const uint32_t a=r->phase_a,b=r->phase_b,c=r->phase_c,sum=a+b+c;
    if(!a || !b || !c || a>=4095U || b>=4095U || c>=4095U ||
       (a==b && b==c) || (uint32_t)(r->completion_tick-r->acquisition_tick)>32U){am13e_coast_phase_reset();return;}
    current_valid=0U;
    state.sample=r->sample_count;state.tick=r->acquisition_tick;state.epoch=epoch;
    last_completion=r->completion_tick;
    state.phase[0]=(uint16_t)a;state.phase[1]=(uint16_t)b;state.phase[2]=(uint16_t)c;
    /* An unknown crossing can hide direction changes or skipped sectors.
     * A fresh sequence must be established after it, not bridged through. */
    if(3U*a==sum || 3U*b==sum || 3U*c==sum){am13e_coast_phase_reset();return;}
    const unsigned pattern=(3U*a>sum?1U:0U)|(3U*b>sum?2U:0U)|(3U*c>sum?4U:0U);
    unsigned index=0U;while(index<6U && forward_patterns[index]!=pattern)++index;
    if(index==6U){am13e_coast_phase_reset();return;}
    if(have){
        const unsigned delta=(index+6U-last_index)%6U;
        if(delta==0U)++state.stable;
        else if(delta==1U || delta==5U){
            const int direction=delta==1U?1:-1;
            state.transitions=state.direction==direction?state.transitions+1U:1U;
            state.direction=direction;state.stable=1U;
        }else{state.direction=0;state.transitions=0U;state.stable=1U;}
    }else state.stable=1U;
    have=current_valid=1U;last_index=index;state.pattern=pattern;
}
int am13e_coast_phase_select(uint32_t now,uint32_t epoch,int reverse,const uint16_t reference[3],AM13E_CoastPhase *status)
{
    static const uint16_t sequence[]=REL17_COMMUTATION_SEQUENCE;
    if(status)*status=state;
    if(!reference || !have || !current_valid || reverse || state.direction!=1 ||
       state.transitions<2U || state.stable<2U || state.epoch!=epoch ||
       (uint32_t)(now-state.tick)>16U)return 0;
    const unsigned step=last_index==5U?1U:last_index+2U;
    const unsigned cc=(sequence[step-1U]>>3U)>>3U,phase=(cc&3U)-1U;
    if(phase>=3U)return 0;
    const unsigned level=state.phase[phase]>reference[phase];
    /* Select the UPCOMING actual edge, not a comparator level already
     * on its post-crossing side. Calibration is acquired, never seeded. */
    if(level==(unsigned)!!(cc&4U))return 0;
    return (int)step;
}
#else
typedef int coast_phase_plan_not_selected;
#endif
