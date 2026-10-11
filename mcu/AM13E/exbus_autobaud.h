/* Original Rel17 EXBUS exbusfunc(): initial n=10 at 125k,
 * on malformed/CRC-failed idle frame --n, alternating 250/125k
 * while n>0; a CRC-valid 0x31 Channel frame sets n=0 permanently.
 * This is an MCU-neutral policy, not physical UART timing proof.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t attempts_remaining;
    bool channels_seen;
    uint32_t baud;
} AM13E_ExbusAutobaud;
void am13e_exbus_autobaud_init(AM13E_ExbusAutobaud *s);
void am13e_exbus_autobaud_valid(AM13E_ExbusAutobaud *s,
                               bool channel_frame);
/* Returns true and a required new bitrate ONLY for an actual
 * malformed/CRC-invalid frame while the original retry budget lasts.
 */
bool am13e_exbus_autobaud_bad(AM13E_ExbusAutobaud *s,
                              uint32_t *requested_baud);
