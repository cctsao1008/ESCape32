#include "telem_tx_progress.h"
unsigned am13e_telem_tx_chunk(unsigned queued,unsigned completed,
                              unsigned max_wire_bytes)
{
    if(completed>=queued || max_wire_bytes==0U)return 0U;
    const unsigned remaining=queued-completed;
    return remaining>max_wire_bytes?max_wire_bytes:remaining;
}
