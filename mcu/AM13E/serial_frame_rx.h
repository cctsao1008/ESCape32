/* ESCape32 Rel17: transport framing for native iBUS/SBUS byte streams.
 * The actual protocol parsers remain in ibus_receiver/sbus_receiver,
 * with original throttle policy in src/io.c. No fake UART stubs.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t bytes[32];
    unsigned used;
    unsigned mode;
    uint32_t frame_errors;
} AM13E_SerialFrameRx;
void am13e_serial_frame_init(AM13E_SerialFrameRx *rx,unsigned mode);
void am13e_serial_frame_abort(AM13E_SerialFrameRx *rx);
bool am13e_serial_frame_byte(AM13E_SerialFrameRx *rx,uint8_t byte,
                             uint8_t ready[32],unsigned *length);
