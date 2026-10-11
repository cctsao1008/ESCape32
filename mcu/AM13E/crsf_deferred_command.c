/* IRQ->PendSV single-slot mailbox; busy remains true while
 * execcrsfcmd() runs. Only PendSV can release the transaction.
 */
#include "crsf_deferred_command.h"
#include <stddef.h>
#include <string.h>
bool am13e_crsf_deferred_begin(AM13E_CrsfDeferredCommand *q,
                               const uint8_t *p,unsigned n)
{
    if(!q||!p||n<4U||n>AM13E_CRSF_PENDING_MAX)return false;
    if(q->busy){
        ++q->drops;
        return false;
    }
    memcpy(q->frame,p,n);
    q->length=n;
    q->busy=1U;
    return true;
}
bool am13e_crsf_deferred_take(AM13E_CrsfDeferredCommand *q,
                              uint8_t *out,unsigned capacity,
                              unsigned *length)
{
    if(!q||!out||!length||!q->busy||q->length==0U||
       capacity<q->length)return false;
    memcpy(out,q->frame,q->length);
    *length=q->length;
    q->length=0U;
    return true;
}
void am13e_crsf_deferred_finish(AM13E_CrsfDeferredCommand *q)
{
    if(!q||!q->busy||q->length!=0U)return;
    memset(q->frame,0,sizeof q->frame);
    q->busy=0U;
}
