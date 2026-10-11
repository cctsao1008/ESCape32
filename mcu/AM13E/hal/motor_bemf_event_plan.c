#include "motor_bemf_event_plan.h"
#include <limits.h>
#include <stddef.h>
AM13E_BemfSample am13e_bemf_sample_plan(uint32_t ticks,uint32_t per_ms,
                                        uint32_t timeout,int *us)
{
    if (!per_ms || !timeout || us==NULL) return AM13E_BEMF_SAMPLE_INVALID;
    /* Timeout before narrow conversion; a stale 32-bit capture is NOT
     * a valid early edge and must not wrap signed microseconds. */
    if (ticks>=timeout) return AM13E_BEMF_SAMPLE_TIMEOUT;
    if (!ticks) return AM13E_BEMF_SAMPLE_INVALID;
    const uint64_t value=((uint64_t)ticks*1000U+per_ms/2U)/per_ms;
    if (!value || value>INT_MAX) return AM13E_BEMF_SAMPLE_INVALID;
    *us=(int)value;
    return AM13E_BEMF_SAMPLE_EDGE;
}
