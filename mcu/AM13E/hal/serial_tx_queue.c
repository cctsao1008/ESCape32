/* Generic byte queue shared by Host tests and actual UNICOMM FIFO IRQ.
 * A second command cannot overwrite an active response. The EOT IRQ,
 * not the point where the last byte is queued, ends the transaction.
 */
#include "serial_tx_queue.h"
#include <stddef.h>
#include <string.h>
bool am13e_serial_tx_queue_begin(AM13E_SerialTxQueue *q,
                                 const uint8_t *p,unsigned n)
{
    if(!q||!p||n==0U||n>AM13E_SERIAL_TX_MAX)return false;
    if(q->count!=0U){
        ++q->dropped;
        return false;
    }
    memcpy(q->data,p,n);
    q->count=n;
    q->sent=0U;
    q->awaiting_eot=false;
    return true;
}
bool am13e_serial_tx_queue_next(AM13E_SerialTxQueue *q,uint8_t *out)
{
    if(!q||!out||q->count==0U||q->sent==q->count)return false;
    *out=q->data[q->sent++];
    if(q->sent==q->count)q->awaiting_eot=true;
    return true;
}
bool am13e_serial_tx_queue_waiting_eot(const AM13E_SerialTxQueue *q)
{
    return q&&q->count!=0U&&q->awaiting_eot;
}
void am13e_serial_tx_queue_eot(AM13E_SerialTxQueue *q)
{
    if(!am13e_serial_tx_queue_waiting_eot(q))return;
    memset(q->data,0,sizeof q->data);
    q->count=0U;
    q->sent=0U;
    q->awaiting_eot=false;
}
