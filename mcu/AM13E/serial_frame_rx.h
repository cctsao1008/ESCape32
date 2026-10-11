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
    uint32_t previous_byte_tick;
    bool has_previous_byte_tick;
} AM13E_SerialFrameRx;
void am13e_serial_frame_init(AM13E_SerialFrameRx *rx,unsigned mode);
void am13e_serial_frame_abort(AM13E_SerialFrameRx *rx);
/* Compare 16kHz SysTick epochs immediately before each FIFO byte.
 * An idle interval >= min_ticks invalidates *only incomplete* RX
 * frames; wraparound is intentional unsigned subtraction.
 * NO motor policy, CRC or UART timing is inferred from this.
 */
void am13e_serial_frame_gap(AM13E_SerialFrameRx *rx,
                             uint32_t now,unsigned min_ticks);
bool am13e_serial_frame_byte(AM13E_SerialFrameRx *rx,uint8_t byte,
                             uint8_t *ready,unsigned capacity,
                             unsigned *length);
