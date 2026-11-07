/* Actual Core PendSV with only external I/O endpoints mocked. */
#include "../../../../../src/main.c"
#include <assert.h>
#include <stdio.h>
static unsigned adc_calls,telemetry_calls;
void adctrig(void) { ++adc_calls; }
void sendtelem(void) { ++telemetry_calls; }
int main(void)
{
    for(unsigned n=0;n<32U;++n){tick=n;pend_sv_handler();}
    assert(telemetry_calls==32U);
#ifdef AM13E_SIM_ONLY_ADC_INTERLEAVE
    assert(adc_calls==4U);
#else
    assert(adc_calls==2U);
#endif
    puts("PASS actual PendSV ADC cadence, production1kHz/explicit SIM_ONLY2kHz, unchanged telemetry dispatch");
    return 0;
}
