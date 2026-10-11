/* Original Rel17 iBUS/SBUS/CRSF native byte-stream framing.
 * Physical UART idle timeout/error abort remains transport-owned.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AM13E_SERIAL_MAX_FRAME 64U
typedef struct {
    uint8_t bytes[AM13E_SERIAL_MAX_FRAME];
    unsigned used;
    unsigned expected;
    unsigned mode;
    uint32_t frame_errors;
} AM13E_SerialFrameRx;
void am13e_serial_frame_init(AM13E_SerialFrameRx *rx,unsigned mode);
void am13e_serial_frame_abort(AM13E_SerialFrameRx *rx);
bool am13e_serial_frame_byte(AM13E_SerialFrameRx *rx,uint8_t byte,
                             uint8_t *ready,unsigned capacity,
                             unsigned *length);
