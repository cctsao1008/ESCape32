/* ESCape32 Rel17 AM13E PB14 capture decoder (no TI register ownership).
 * Input timestamps come from successive hardware ECAP0 capture events.
 * DShot output generation is intentionally NOT part of this RX decoder.
 */
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t last_start;
    uint32_t last_end;
    uint32_t pending_width;
    uint32_t last_bit_period;
    uint32_t tick_hz;
    uint32_t good_pwm;
    uint32_t good_dshot;
    uint32_t rejected;
    uint16_t bits;
    uint8_t decoded_bits;
    uint8_t active;
    uint8_t inverted;
} AM13E_PB14_Decoder;

/* Conservative four-edge capture sanity check. Uses calibrated eCAP Hz.
 * Recognizes rollover; rejects impossible pulse widths or edge ordering.
 * Returns 0 on uncalibrated clock or invalid group.
 */
int am13e_command_capture_group_valid(uint32_t start1, uint32_t end1,
                                   uint32_t start2, uint32_t end2,
                                   uint32_t tick_hz);

/* Protocol dispatch owns the original ESCape32 policy, not this decoder. */
typedef void (*AM13E_PB14_PwmCallback)(unsigned int pulse_us);
typedef int (*AM13E_PB14_DshotCallback)(uint16_t frame, int inverted);

void am13e_command_decoder_reset(AM13E_PB14_Decoder *d, uint32_t tick_hz, int inverted);
void am13e_command_decoder_pulse(AM13E_PB14_Decoder *d, uint32_t start,
                              uint32_t end, AM13E_PB14_PwmCallback pwm,
                              AM13E_PB14_DshotCallback dshot);
/* Drop a suspect capture group without retaining a partial DShot frame.
 * Preserve raw clock calibration and diagnostic totals.
 */
void am13e_command_decoder_abort(AM13E_PB14_Decoder *d);

void am13e_command_decoder_idle(AM13E_PB14_Decoder *d, uint32_t now,
                             AM13E_PB14_DshotCallback dshot);
