/* Original Rel17 iobuf[128] pending-tail and 64B UC FIFO contract.
 * Every EOT drains one bounded segment, retaining byte ordering.
 */
#pragma once
unsigned am13e_telem_tx_chunk(unsigned queued,unsigned completed,
                              unsigned max_wire_bytes);
