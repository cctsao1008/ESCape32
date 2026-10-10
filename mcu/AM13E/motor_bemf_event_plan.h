/* Rel17 BEMF candidate and zero-cross control planning.
 * Used in REAL AM13E ECAP1 ISR and src/main.c shared Rel17 policy.
 * Hardware filter/phase mapping remains in motor_bemf.c.
 */
#pragma once
#include <stdint.h>
typedef enum {
    AM13E_BEMF_SAMPLE_INVALID=0,
    AM13E_BEMF_SAMPLE_EDGE=1,
    AM13E_BEMF_SAMPLE_TIMEOUT=2
} AM13E_BemfSample;
AM13E_BemfSample am13e_bemf_sample_plan(
    uint32_t ticks,uint32_t ticks_per_ms,uint32_t timeout_ticks,int *us);
typedef struct { int interval_us,delay_us,fast; } AM13E_BemfPolicyPlan;
/* 1 = accepted and filled output, 0 = early (state unchanged),
 * -1 = invalid arithmetic/input. Only accepted edges arm TIMG12.
 */
int am13e_bemf_policy_plan(int ival,int ertm,int captured,int timing,
                          AM13E_BemfPolicyPlan *out);
