#pragma once
#ifdef AM13E_SIM_ONLY_RESTART_PHASE_RATE
#include "analog_runtime.h"
/* Experimental 1kHz apparent vector rotation, NOT a qualified direction sensor.
 * Above-Nyquist rotation can alias: callers must not claim physical direction
 * qualification from this estimator. No motor angle or speed is an input. */
void am13e_restart_rate_update(const AM13E_AdcRaw *raw,uint32_t epoch);
void am13e_restart_rate_invalidate(void);
int am13e_restart_rate_period(uint32_t now,uint32_t epoch,int reverse);
#endif
