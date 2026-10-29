/*
 * E62 experimental passive three-phase windmill observation contract.
 * NON-ACTUATING: never arm PWM, select a startup sector or infer a
 * physical rotor angle from unverified ADC channel/routing evidence.
 *
 * The caller is responsible for documented board phase order, VDDA/ADC
 * calibration, acquisition timing and analog qualification. A software
 * candidate alone cannot authorize the real power stage.
 */
#pragma once
#include <stdint.h>

typedef enum {
    AM13E_WINDMILL_REJECT=0,
    AM13E_WINDMILL_COLLECT=1,
    AM13E_WINDMILL_CANDIDATE_ONLY=2
} AM13E_WindmillVerdict;

typedef struct {
    uint32_t max_sample_age_ticks;
    uint32_t max_adc_scan_ticks;
    uint32_t max_sample_gap_ticks;
    uint16_t min_adjacent_phase_delta_codes;
    uint8_t min_consistent_transitions; /* at least 3, no guessed confidence */
    uint8_t board_phase_route_verified;
    uint8_t board_adc_reference_verified;
    uint8_t board_sample_clock_verified;
} AM13E_WindmillContract;

typedef struct {
    uint16_t phase[3]; /* actual complete three-phase ADC sample; no model theta */
    uint32_t sample_id;
    uint32_t acquisition_tick;
    uint32_t completion_tick;
    /* Explicit passive sampling contract, supplied by real HW owner.
     * Never infer these booleans from the QEMU/model BEMF code values.
     */
    uint8_t coherent_three_phase_scan;
    uint8_t six_outputs_inactive;
    uint8_t trip_latch_clear;
} AM13E_WindmillSample;

typedef struct {
    uint32_t last_sample_id;
    uint32_t last_completion_tick;
    uint8_t previous_sector;
    int8_t direction; /* +1/-1 six-rank transitions; not rotor direction */
    uint8_t transitions;
    uint8_t seen_first;
    uint8_t rejected;
} AM13E_WindmillObserver;

void am13e_windmill_observer_reset(AM13E_WindmillObserver *observer);
AM13E_WindmillVerdict am13e_windmill_observe(
    AM13E_WindmillObserver *observer,
    const AM13E_WindmillContract *contract,
    const AM13E_WindmillSample *sample,
    uint32_t now_tick);

/* A model/snapshot-based observer must NEVER grant physical motor rearm. */
int am13e_windmill_motor_rearm_permitted(const AM13E_WindmillObserver *observer);
