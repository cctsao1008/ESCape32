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
int am13e_bemf_policy_plan(int ival,int ertm,int captured,int timing,
                          AM13E_BemfPolicyPlan *out)
{
    if (!out || ival<=0 || captured<=0 || ival>INT_MAX/3 ||
        timing<0 || timing>32) return -1;
    if (captured < (ival>>1)) return 0;
    const int u=ival*3;
    if (captured>INT_MAX-u) return -1;
    AM13E_BemfPolicyPlan p={0};
    /* Exact original Rel17 integer ordering and advance formula. */
    p.fast=(captured<(u>>2)||captured>(u>>1))&&ertm<2000;
    p.interval_us=(captured+u)>>2;
    if (p.interval_us>INT_MAX/32) return -1;
    p.delay_us=(p.interval_us-((p.interval_us*timing)>>5))>>1;
    if (p.delay_us<1) p.delay_us=1;
    *out=p;
    return 1;
}
