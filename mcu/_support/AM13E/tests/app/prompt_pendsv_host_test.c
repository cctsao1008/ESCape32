/* UNIT source worker dispatch only, no hardware measurements or fake IRQs. */
#include "irq_vectors.h"
#include <assert.h>
#include <stdio.h>
static unsigned reference_calls,core_calls,saves;
void am13e_app_adc_reference_service(void){++reference_calls;assert(core_calls+1U==reference_calls);}
void pend_sv_handler(void){++core_calls;}
void am13e_app_io_service_pending_save(void){++saves;}
int main(void){
    for(unsigned i=0;i<32U;++i)PendSV_Handler();
    assert(core_calls==32U && saves==32U);
#ifdef AM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER
    assert(reference_calls==32U);
#else
    assert(reference_calls==0U);
#endif
    puts("PASS actual PendSV bridge: prompt service before unchanged Core dispatch, no new ADC/interrupt source");
    return 0;
}
