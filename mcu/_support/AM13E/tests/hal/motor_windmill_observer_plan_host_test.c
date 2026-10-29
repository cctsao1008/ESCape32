/* Pure-C E62 #5 observer: intentionally NO motor output or silicon IO. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "motor_windmill_observer_plan.h"

static const AM13E_WindmillContract valid={
    .max_sample_age_ticks=50U,.max_adc_scan_ticks=4U,
    .max_sample_gap_ticks=20U,.min_adjacent_phase_delta_codes=3U,
    .min_consistent_transitions=3U,
    .board_phase_route_verified=1U,.board_adc_reference_verified=1U,
    .board_sample_clock_verified=1U
};
static AM13E_WindmillSample sample(unsigned rank,uint32_t id,uint32_t end)
{
    static const uint8_t order[6][3]={
        {0,1,2},{0,2,1},{2,0,1},{2,1,0},{1,2,0},{1,0,2}
    };
    AM13E_WindmillSample s={
        .sample_id=id,.acquisition_tick=end-2U,.completion_tick=end
    };
    s.phase[order[rank][0]]=2100U;
    s.phase[order[rank][1]]=2048U;
    s.phase[order[rank][2]]=2000U;
    return s;
}
static void good_sequence(void)
{
    AM13E_WindmillObserver o={0};
    for(unsigned i=0;i<4;i++){
        AM13E_WindmillSample s=sample(i,11U+i,100U+10U*i);
        const AM13E_WindmillVerdict v=am13e_windmill_observe(&o,&valid,&s,s.completion_tick);
        assert(v==(i==3U?AM13E_WINDMILL_CANDIDATE_ONLY:AM13E_WINDMILL_COLLECT));
        assert(am13e_windmill_motor_rearm_permitted(&o)==0);
    }
    printf("PASS E62 candidate-only source phase sequence with NO PWM rearm\n");
}
static void reject_bad_samples(void)
{
    AM13E_WindmillObserver o={0};
    AM13E_WindmillSample s=sample(0,1U,100U);
    AM13E_WindmillContract c=valid;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_COLLECT);
    s=sample(1,2U,110U);
    s.phase[0]=s.phase[1]; /* quantization/tie */
    assert(am13e_windmill_observe(&o,&c,&s,110U)==AM13E_WINDMILL_REJECT);
    s=sample(1,2U,110U);
    assert(am13e_windmill_observe(&o,&c,&s,110U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    s.phase[0]=4096U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    s.sample_id=0U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    assert(am13e_windmill_observe(&o,&c,NULL,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_COLLECT);
    s=sample(3,2U,110U); /* skip two ranked sectors */
    assert(am13e_windmill_observe(&o,&c,&s,110U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_COLLECT);
    s=sample(1,2U,110U);
    assert(am13e_windmill_observe(&o,&c,&s,110U)==AM13E_WINDMILL_COLLECT);
    s=sample(0,3U,120U); /* reverses previous rank direction */
    assert(am13e_windmill_observe(&o,&c,&s,120U)==AM13E_WINDMILL_REJECT);
    puts("PASS E62 rejects ambiguous codes, bad identities, phase jumps and direction disagreement");
}
static void reject_bad_epochs(void)
{
    AM13E_WindmillObserver o={0};
    AM13E_WindmillSample s=sample(0,1U,100U);
    AM13E_WindmillContract c=valid;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_COLLECT);
    s=sample(1,3U,110U); /* skipped ADC epoch */
    assert(am13e_windmill_observe(&o,&c,&s,110U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_COLLECT);
    s=sample(1,2U,131U); /* gap too long */
    assert(am13e_windmill_observe(&o,&c,&s,131U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    s.completion_tick=102U; /* completion in future */
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    s.acquisition_tick=110U; /* conversion began in the future */
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,100U);
    assert(am13e_windmill_observe(&o,&c,&s,151U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    s=sample(0,1U,UINT32_MAX-3U);
    assert(am13e_windmill_observe(&o,&c,&s,UINT32_MAX-1U)==AM13E_WINDMILL_COLLECT);
    s=sample(1,2U,4U); /* legitimate 32-bit clock wrap */
    assert(am13e_windmill_observe(&o,&c,&s,5U)==AM13E_WINDMILL_COLLECT);
    puts("PASS E62 acquisition/completion chronology, identity/gap, stale and wraparound gates");
}
static void reject_unverified_board(void)
{
    AM13E_WindmillObserver o={0};
    AM13E_WindmillSample s=sample(0,1U,100U);
    AM13E_WindmillContract c=valid;
    c.board_adc_reference_verified=0U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    c=valid;c.board_phase_route_verified=0U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    c=valid;c.board_sample_clock_verified=0U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    c=valid;c.max_sample_age_ticks=UINT32_MAX;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    am13e_windmill_observer_reset(&o);
    c=valid;c.min_consistent_transitions=2U;
    assert(am13e_windmill_observe(&o,&c,&s,100U)==AM13E_WINDMILL_REJECT);
    assert(am13e_windmill_motor_rearm_permitted(NULL)==0);
    puts("PASS E62 source board-route/calibration/cadence contract incomplete -> fail closed");
}
int main(void)
{
    good_sequence();
    reject_bad_samples();
    reject_bad_epochs();
    reject_unverified_board();
    return 0;
}
