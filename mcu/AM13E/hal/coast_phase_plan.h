#pragma once
/* SIM_ONLY research observer. A phase sequence is inferred from sampled
 * coast voltages; above-band aliases and real per-phase offset/noise are
 * not qualified here. The native experiments separately verify gate-off
 * samples. Do not treat this API as a general physical direction sensor. */
#ifdef AM13E_SIM_ONLY_COAST_PHASE_ACQUIRE
#include "analog_runtime.h"
typedef struct {uint32_t sample,tick,epoch,pattern,stable,transitions;int direction;uint16_t phase[3];} AM13E_CoastPhase;
void am13e_coast_phase_reset(void);
void am13e_coast_phase_update(const AM13E_AdcRaw *raw,uint32_t epoch);
int am13e_coast_phase_select(uint32_t now,uint32_t epoch,int reverse,const uint16_t reference[3],AM13E_CoastPhase *status);
#endif
