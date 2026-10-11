/* Original Rel17 src/io.c::serialfunc (input_mode=2) policy.
 * Native UART byte transport is selected separately by a board profile.
 * No new CLI syntax, telemetry source, or GPIO routing is invented.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t (*AM13E_SerialCrc8)(const char *,int);
typedef struct {
    int throttle;
    int brake;
    int auxup;
    int flipdir;
    int led;
    int duty_drag;
    int led_count;  /* 0 means no selected physical LED backend */
    int temp1,temp2,volt,curr,csum,ertm;
} AM13E_SerialCommandState;
/* Return false only for an invalid wire header/CRC/argument. For a
 * CRC-valid command, update state like original serialfunc and set
 * reply_length to 0/4/12; malformed token ends processing as Rel17.
 */
bool am13e_serial_command(const uint8_t *frame,unsigned length,
                          AM13E_SerialCommandState *state,
                          uint8_t *reply,unsigned capacity,
                          unsigned *reply_length,AM13E_SerialCrc8 crc);
