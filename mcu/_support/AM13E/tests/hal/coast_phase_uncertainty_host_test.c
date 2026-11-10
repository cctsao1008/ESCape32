/* UNIT counterexamples: these vectors never enter a native motor run. */
#include "coast_phase_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static AM13E_AdcRaw raw;
static void put(unsigned pattern){
    raw.phase_a=1000U+!!(pattern&1U);raw.phase_b=1000U+!!(pattern&2U);raw.phase_c=1000U+!!(pattern&4U);
    ++raw.sample_count;raw.acquisition_tick+=16U;raw.completion_tick=raw.acquisition_tick;
    am13e_coast_phase_update(&raw,3);
}
int main(int argc,char **argv){
    const uint16_t reference[]={1000,1000,1000};
    const unsigned mode=argc>1?(unsigned)atoi(argv[1]):0;
    am13e_coast_phase_reset();put(5);put(5);put(1);put(1);put(3);put(3);
    assert(am13e_coast_phase_select(raw.acquisition_tick,3,0,reference,0)==4);
    if(mode==0){ /* Same identity, contradictory phase payload. */
        raw.phase_a=1001;raw.phase_b=1000;raw.phase_c=1001;
        am13e_coast_phase_update(&raw,3);
    } else if(mode==1){ /* Unknown phase may hide arbitrary transitions. */
        ++raw.sample_count;raw.acquisition_tick+=16;raw.completion_tick=raw.acquisition_tick;
        raw.phase_a=1000;raw.phase_b=1001;raw.phase_c=1002;
        am13e_coast_phase_update(&raw,3);put(3);put(3);
    } else if(mode==2){ /* A duplicate acquisition identity cannot move time. */
        raw.acquisition_tick+=1;raw.completion_tick+=1;am13e_coast_phase_update(&raw,3);
    } else if(mode==3){ /* Exact duplicate must remain idempotent. */
        AM13E_CoastPhase before,after;
        assert(am13e_coast_phase_select(raw.acquisition_tick,3,0,reference,&before));
        am13e_coast_phase_update(&raw,3);
        assert(am13e_coast_phase_select(raw.acquisition_tick,3,0,reference,&after)==4);
        assert(before.stable==after.stable && before.transitions==after.transitions);
        puts("PASS_NEW identical epoch replay remains idempotent");return 0;
    } else return 2;
    assert(!am13e_coast_phase_select(raw.acquisition_tick,3,0,reference,0));
    puts("PASS_NEW ambiguous or contradictory evidence invalidates authorization");return 0;
}
