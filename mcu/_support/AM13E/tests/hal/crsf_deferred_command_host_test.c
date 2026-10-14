/* Real IRQ/PendSV mailbox model with bounded no-overwrite semantics. */
#include "crsf_deferred_command.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
int main(void)
{
    AM13E_CrsfDeferredCommand q={0};
    uint8_t frame[64]={0xc8U,4U,0x2dU,1U,5U,0U},out[64]={0};
    unsigned length=0U;
    assert(!am13e_crsf_deferred_begin(NULL,frame,6U));
    assert(!am13e_crsf_deferred_begin(&q,NULL,6U));
    assert(!am13e_crsf_deferred_begin(&q,frame,65U));
    assert(am13e_crsf_deferred_begin(&q,frame,6U));
    assert(q.busy==1U&&q.length==6U);
    frame[2]=0x28U;
    assert(!am13e_crsf_deferred_begin(&q,frame,6U));
    assert(q.drops==1U&&q.frame[2]==0x2dU);
    assert(!am13e_crsf_deferred_take(&q,out,5U,&length));
    assert(am13e_crsf_deferred_take(&q,out,64U,&length));
    assert(length==6U&&out[2]==0x2dU&&q.busy==1U&&q.length==0U);
    assert(!am13e_crsf_deferred_begin(&q,frame,6U));
    assert(q.drops==2U);
    assert(!am13e_crsf_deferred_take(&q,out,64U,&length));
    am13e_crsf_deferred_finish(&q);
    assert(!q.busy&&!q.length);
    for(unsigned i=0;i<64U;++i)assert(q.frame[i]==0U);
    assert(am13e_crsf_deferred_begin(&q,frame,6U));
    assert(am13e_crsf_deferred_take(&q,out,64U,&length));
    am13e_crsf_deferred_finish(&q);
    puts("PASS CRSF RX IRQ to PendSV command mailbox; busy during execution");
    return 0;
}
