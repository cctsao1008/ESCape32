/* Original Rel17 src/io.c::sbusfunc/sbusirq2 source semantics.
 * Six 3-byte physical slot payloads, 3200us initial and 660us
 * spacing. This planner DOES NOT authorize or implement physical
 * inverted 8E2 reply slots: a qualified timed board transport
 * must enforce the original USART RX disable/enable and delay.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t slot_index;
} AM13E_Sbus2ReplyState;
typedef struct {
    int temp1,temp2,curr,volt,csum,erpm,telem_poles;
    unsigned telphid;
} AM13E_Sbus2ReplyValues;
bool am13e_sbus2_eligible(const uint8_t *frame,unsigned length,
                          unsigned telmode,unsigned telphid);
bool am13e_sbus2_next_packet(AM13E_Sbus2ReplyState *state,
                             const AM13E_Sbus2ReplyValues *values,
                             uint8_t out[3],unsigned *delay_us);
