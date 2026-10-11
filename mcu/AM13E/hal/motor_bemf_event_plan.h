/* Hardware ECAP candidate conversion to the Core microsecond domain.
 * Zero-cross control policy is the original src/main.c function.
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
