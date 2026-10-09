/* Protocol-independent hardware timestamp validation for PB14 RX.
 * A complete DShot packet enters the existing Rel17 CRC/throttle callback.
 * This decoder does not fabricate a watchdog feed, telemetry or PWM output.
 */
#include "pb14_decode.h"
#include <stddef.h>

static void clear_frame(AM13E_PB14_Decoder *d)
{
    d->bits = 0U;
    d->decoded_bits = 0U;
    d->last_bit_period = 0U;
}

void am13e_pb14_decoder_reset(AM13E_PB14_Decoder *d, uint32_t tick_hz, int inverted)
{
    if (d == NULL) return;
    *d = (AM13E_PB14_Decoder){0};
    d->tick_hz = tick_hz;
    d->inverted = !!inverted;
}

static int bit_push(AM13E_PB14_Decoder *d, uint32_t width, uint32_t period)
{
    if (!period || d->decoded_bits >= 16U ||
        (uint64_t)width * 100U < (uint64_t)period * 20U ||
        (uint64_t)width * 100U > (uint64_t)period * 88U) return 0;
    const unsigned bit = ((uint64_t)width * 100U >= (uint64_t)period * 55U);
    d->bits = (uint16_t)((d->bits << 1) | bit);
    ++d->decoded_bits;
    d->last_bit_period = period;
    return 1;
}

static void finish_frame(AM13E_PB14_Decoder *d, AM13E_PB14_DshotCallback cb)
{
    if (d->decoded_bits == 15U && d->last_bit_period) {
        if (!bit_push(d, d->pending_width, d->last_bit_period)) {
            ++d->rejected;
            clear_frame(d);
            return;
        }
    }
    if (d->decoded_bits == 16U && cb != NULL && cb(d->bits, d->inverted)) {
        ++d->good_dshot;
    } else if (d->decoded_bits != 0U) {
        ++d->rejected;
    }
    clear_frame(d);
}

void am13e_pb14_decoder_pulse(AM13E_PB14_Decoder *d, uint32_t start,
                              uint32_t end, AM13E_PB14_PwmCallback pwm,
                              AM13E_PB14_DshotCallback dshot)
{
    if (d == NULL || d->tick_hz == 0U) return;
    const uint32_t width = end - start;
    if (width == 0U) { ++d->rejected; return; }
    if (!d->active) {
        d->active = 1U;
        d->last_start = start;
        d->last_end = end;
        d->pending_width = width;
        return;
    }
    const uint32_t period = start - d->last_start;
    const uint32_t us_ticks = d->tick_hz / 1000000U;
    if (us_ticks == 0U) { ++d->rejected; return; }

    /* 2.5..50ms period and 800..2200us pulse: legacy servo PWM only.
     * Validating the period necessarily delays the first pulse one frame.
     */
    if (period >= 2500U * us_ticks && period <= 50000U * us_ticks &&
        d->pending_width >= 800U * us_ticks &&
        d->pending_width <= 2200U * us_ticks) {
        clear_frame(d);
        const unsigned usec = (unsigned)((d->pending_width + us_ticks / 2U) / us_ticks);
        if (pwm != NULL) pwm(usec);
        ++d->good_pwm;
    } else if (period >= us_ticks / 2U && period <= 9U * us_ticks &&
               d->pending_width < period &&
               (d->last_bit_period == 0U ||
                ((uint64_t)period * 5U >= (uint64_t)d->last_bit_period * 4U &&
                 (uint64_t)period * 5U <= (uint64_t)d->last_bit_period * 6U))) {
        if (!bit_push(d, d->pending_width, period)) {
            ++d->rejected;
            clear_frame(d);
        }
    } else {
        /* DShot packet ended, or input timing changed. A frame has 16
         * pulses; accept nothing without a full frame and Rel17 CRC.
         */
        if (d->decoded_bits) finish_frame(d, dshot);
        else clear_frame(d);
    }
    d->last_start = start;
    d->last_end = end;
    d->pending_width = width;
}

void am13e_pb14_decoder_idle(AM13E_PB14_Decoder *d, uint32_t now,
                             AM13E_PB14_DshotCallback dshot)
{
    if (d == NULL || !d->active || !d->last_bit_period) return;
    /* End-of-frame software timeout. This RX-only stage cannot meet
     * the BiDShot TX turnaround timing and MUST NOT drive the line.
     */
    if ((uint32_t)(now - d->last_end) > d->last_bit_period * 4U) {
        finish_frame(d, dshot);
        d->active = 0U;
    }
}
