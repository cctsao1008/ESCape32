/* UNIT samples only. Native experiments consume only real Guest ADC EOC. */
#include "coast_phase_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static const unsigned patterns[]={5,1,3,2,6,4};
static uint32_t sample,tick_now;
static void sample_pattern(unsigned code,uint32_t epoch)
{
    AM13E_AdcRaw r={0};r.phase_a=(uint16_t)(1000U+!!(code&1U));r.phase_b=(uint16_t)(1000U+!!(code&2U));r.phase_c=(uint16_t)(1000U+!!(code&4U));
    r.sample_count=++sample;r.acquisition_tick=r.completion_tick=(tick_now+=16U);am13e_coast_phase_update(&r,epoch);
}
static void seed(unsigned endpoint)
{
    am13e_coast_phase_reset();sample=0;tick_now=0;
    for(unsigned k=0;k<3;++k){unsigned i=(endpoint+4U+k)%6U;sample_pattern(patterns[i],3);sample_pattern(patterns[i],3);}
}
int main(void)
{
    const uint16_t reference[]={1000,1000,1000};AM13E_CoastPhase info;
    for(unsigned i=0;i<6;++i){seed(i);assert(am13e_coast_phase_select(tick_now,3,0,reference,&info)==(i==5?1:(int)i+2));assert(info.direction==1 && info.transitions==2 && info.stable==2);
        assert(!am13e_coast_phase_select(tick_now+17U,3,0,reference,0));assert(!am13e_coast_phase_select(tick_now,4,0,reference,0));assert(!am13e_coast_phase_select(tick_now,3,1,reference,0));}
    am13e_coast_phase_reset();assert(!am13e_coast_phase_select(tick_now,3,0,reference,0));
    sample_pattern(5,3);sample_pattern(5,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,0));
    sample_pattern(4,3);sample_pattern(4,3);sample_pattern(6,3);sample_pattern(6,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,&info) && info.direction==-1);
    seed(2);sample_pattern(4,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,0)); /* nonadjacent */
    seed(2);sample+=2;sample_pattern(3,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,0));
    seed(2);sample_pattern(3,4);assert(!am13e_coast_phase_select(tick_now,4,0,reference,0));
    seed(2);sample_pattern(0,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,0)); /* all equal */
    seed(2);AM13E_AdcRaw r={0};r.phase_a=1000;r.phase_b=1001;r.phase_c=1002;r.sample_count=++sample;r.acquisition_tick=r.completion_tick=(tick_now+=16);am13e_coast_phase_update(&r,3);assert(!am13e_coast_phase_select(tick_now,3,0,reference,0));
    seed(1);const uint16_t wrong_reference[]={999,999,999};assert(!am13e_coast_phase_select(tick_now,3,0,wrong_reference,0)); /* rising already high */
    seed(0);const uint16_t high_reference[]={1002,1002,1002};assert(!am13e_coast_phase_select(tick_now,3,0,high_reference,0)); /* falling already low */
    puts("PASS_NEW six sectors, actual upcoming edge, forward sequence, reverse/missing/static/jump/gap/epoch/stale/equal/center/reference negatives");
    puts("INCOMPLETE physical direction proof: sampled phase sequence admits above-band alias and native ideal1LSB codes are not analog noise qualification");
    return 0;
}
