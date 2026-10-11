#include "telem_tx_progress.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for(unsigned total=0U;total<=128U;++total){
        unsigned sent=0U,count=0U;
        while(sent<total){
            const unsigned n=am13e_telem_tx_chunk(total,sent,64U);
            assert(n>0U&&n<=64U&&sent+n<=total);
            sent+=n;++count;
        }
        assert(sent==total&&count==(total+63U)/64U);
    }
    assert(am13e_telem_tx_chunk(12U,13U,64U)==0U);
    assert(am13e_telem_tx_chunk(12U,0U,0U)==0U);
    assert(am13e_telem_tx_chunk(12U,8U,64U)==4U);
    puts("PASS original Rel17 128B UART telemetry queue / 64B EOT segmentation");
    return 0;
}
