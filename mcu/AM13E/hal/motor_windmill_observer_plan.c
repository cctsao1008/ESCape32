/* E62 read-only passive rank observer; never an actuator interface. */
#include "motor_windmill_observer_plan.h"
#include <stddef.h>
#include <string.h>
#include <limits.h>

static int contract_valid(const AM13E_WindmillContract *c)
{
    return c && c->board_phase_route_verified==1U &&
        c->board_adc_reference_verified==1U &&
        c->board_sample_clock_verified==1U &&
        c->min_adjacent_phase_delta_codes>0U &&
        c->max_sample_age_ticks>0U &&
        c->max_adc_scan_ticks>0U &&
        c->max_sample_gap_ticks>0U &&
        c->max_sample_age_ticks<=INT32_MAX &&
        c->max_adc_scan_ticks<=c->max_sample_age_ticks &&
        c->max_sample_gap_ticks<=INT32_MAX &&
        c->min_consistent_transitions>=3U;
}

static int rank_sector(const uint16_t phase[3],uint16_t separation)
{
    unsigned order[3]={0U,1U,2U};
    for(unsigned i=0U;i<3U;i++)
        if(phase[i]>4095U)return -1;
    for(unsigned i=0U;i<3U;i++)
        for(unsigned j=i+1U;j<3U;j++)
            if(phase[order[j]]>phase[order[i]]) {
                const unsigned t=order[i];order[i]=order[j];order[j]=t;
            }
    /* Reject equal/quantized ADC codes, not a tie-break guessed sector. */
    if((unsigned)phase[order[0]]-phase[order[1]]<separation ||
       (unsigned)phase[order[1]]-phase[order[2]]<separation)return -1;
    static const unsigned ranks[6][3]={
        {0U,1U,2U},{0U,2U,1U},{2U,0U,1U},
        {2U,1U,0U},{1U,2U,0U},{1U,0U,2U}
    };
    for(unsigned k=0U;k<6U;k++)
        if(order[0]==ranks[k][0]&&order[1]==ranks[k][1]&&
           order[2]==ranks[k][2])return (int)k;
    return -1;
}

void am13e_windmill_observer_reset(AM13E_WindmillObserver *o)
{
    if(o)memset(o,0,sizeof(*o));
}

AM13E_WindmillVerdict am13e_windmill_observe(
    AM13E_WindmillObserver *o,const AM13E_WindmillContract *c,
    const AM13E_WindmillSample *s,uint32_t now)
{
    if(!o)return AM13E_WINDMILL_REJECT;
    if(o->rejected)return AM13E_WINDMILL_REJECT;
    if(!contract_valid(c) || !s || !s->sample_id)goto reject;
    /* All timestamps are the same monotonic wrapping uint32 clock.
     * All modular elapsed intervals must be independently bounded;
     * a future, out-of-order or incomplete conversion fails closed.
     */
    const uint32_t scan=s->completion_tick-s->acquisition_tick;
    const uint32_t age=now-s->completion_tick;
    const uint32_t total=now-s->acquisition_tick;
    if(scan>c->max_adc_scan_ticks || age>c->max_sample_age_ticks ||
       total>c->max_sample_age_ticks)goto reject;
    if(o->seen_first) {
        const uint32_t gap=s->completion_tick-o->last_completion_tick;
        if(s->sample_id!=o->last_sample_id+1U ||
           gap==0U || gap>c->max_sample_gap_ticks)goto reject;
    }
    const int sector=rank_sector(s->phase,c->min_adjacent_phase_delta_codes);
    if(sector<0)goto reject;
    if(o->seen_first && sector!=(int)o->previous_sector) {
        const int delta=(sector-(int)o->previous_sector+6)%6;
        const int8_t sign=(delta==1)?1:(delta==5)?-1:0;
        if(sign==0 || (o->direction && sign!=o->direction))goto reject;
        o->direction=sign;
        if(o->transitions<UINT8_MAX)++o->transitions;
    }
    o->last_sample_id=s->sample_id;
    o->last_completion_tick=s->completion_tick;
    o->previous_sector=(uint8_t)sector;
    o->seen_first=1U;
    return o->transitions>=c->min_consistent_transitions?
        AM13E_WINDMILL_CANDIDATE_ONLY:AM13E_WINDMILL_COLLECT;
reject:
    o->rejected=1U;
    return AM13E_WINDMILL_REJECT;
}

int am13e_windmill_motor_rearm_permitted(const AM13E_WindmillObserver *o)
{
    /* Deliberately unimplemented until a separate hardware-validated
     * arbitration/actuation gate exists. Do not return true from here.
     */
    (void)o;
    return 0;
}
