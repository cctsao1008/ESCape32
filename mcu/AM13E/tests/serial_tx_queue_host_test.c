/* Production queue state: bound, copied bytes, no overwrite, FIFO
 * refill until exactly one EOT finishes last shift-register byte.
 */
#include "serial_tx_queue.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){
    AM13E_SerialTxQueue q={0};
    uint8_t packet[64],out=0xffU;
    for(unsigned i=0;i<64U;++i)packet[i]=(uint8_t)(i^0x5aU);
    assert(!am13e_serial_tx_queue_begin(NULL,packet,64U));
    assert(!am13e_serial_tx_queue_begin(&q,NULL,64U));
    assert(!am13e_serial_tx_queue_begin(&q,packet,0U));
    assert(!am13e_serial_tx_queue_begin(&q,packet,65U));
    assert(am13e_serial_tx_queue_begin(&q,packet,64U));
    memset(packet,0,sizeof packet);
    assert(!am13e_serial_tx_queue_begin(&q,packet,4U));
    assert(q.dropped==1U);
    assert(!am13e_serial_tx_queue_waiting_eot(&q));
    am13e_serial_tx_queue_eot(&q);
    assert(q.count==64U);
    for(unsigned i=0;i<64U;++i){
        assert(am13e_serial_tx_queue_next(&q,&out));
        assert(out==(uint8_t)(i^0x5aU));
    }
    assert(!am13e_serial_tx_queue_next(&q,&out));
    assert(am13e_serial_tx_queue_waiting_eot(&q));
    assert(!am13e_serial_tx_queue_next(&q,NULL));
    assert(!am13e_serial_tx_queue_begin(&q,packet,1U));
    assert(q.dropped==2U);
    am13e_serial_tx_queue_eot(&q);
    assert(q.count==0U && q.sent==0U && !q.awaiting_eot);
    for(unsigned i=0;i<64U;++i)assert(q.data[i]==0U);
    packet[0]=0x28U; packet[1]=0x29U;
    assert(am13e_serial_tx_queue_begin(&q,packet,2U));
    assert(am13e_serial_tx_queue_next(&q,&out)&&out==0x28U);
    assert(am13e_serial_tx_queue_next(&q,&out)&&out==0x29U);
    assert(am13e_serial_tx_queue_waiting_eot(&q));
    am13e_serial_tx_queue_eot(&q);
    puts("PASS UNICOMM TX 64B queue ownership, no overwrite and EOT semantics");
    return 0;
}
