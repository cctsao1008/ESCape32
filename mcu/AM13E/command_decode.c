/* Protocol-independent hardware timestamp validation for PB14 RX.
 * A complete DShot packet enters the existing Rel17 CRC/throttle callback.
 * This decoder does not fabricate a watchdog feed, telemetry or PWM output.
 */
#include "command_decode.h"
#include <stddef.h>

/* Require plausible, forward-ordered edges in one four-event ECAP0
 * group. Subtraction is modulo-2^32, so a genuine TSCTR wrap works.
 * Reject a group spanning an implausibly long pulse or idle gap.
 * This guard cannot detect a COMPLETE four-event register overwrite:
 * only the hardware latency / DMA measurement can establish that.
 */
int am13e_pb14_capture_snapshot_valid(
    uint32_t flags_before,uint32_t flags_after,
    uint32_t required_events,unsigned phase_before,
    unsigned phase_after,unsigned next_expected_phase)
{
    return required_events!=0U &&
           (flags_before&required_events)==required_events &&
           (flags_after&required_events)==0U &&
           phase_before==next_expected_phase &&
           phase_after==next_expected_phase;
}

int am13e_pb14_capture_budget_ok(uint32_t first_start,
                                  uint32_t second_start,
                                  uint32_t final_edge,
                                  uint32_t current_counter,
                                  uint32_t capture_hz)
{
    const uint32_t us_ticks=capture_hz/UINT32_C(1000000);
    if(!us_ticks || us_ticks>200U)return 0;
    const uint32_t bit_ticks=second_start-first_start;
    if(bit_ticks<us_ticks/2U || bit_ticks>us_ticks*9U)
        return 1; /* Separate servo pulses, not a DShot bit pair. */
    return (uint32_t)(current_counter-final_edge)<
           (uint32_t)(bit_ticks*2U);
}

int am13e_pb14_capture_group_valid(uint32_t start1, uint32_t end1,
                                   uint32_t start2, uint32_t end2,
                                   uint32_t tick_hz)
{
    const uint32_t ticks_per_us = tick_hz / UINT32_C(1000000);
    if (!ticks_per_us || ticks_per_us > 200U) return 0;
    const uint32_t width1 = end1 - start1;
    const uint32_t gap = start2 - end1;
    const uint32_t width2 = end2 - start2;
    const uint32_t max_width = ticks_per_us * UINT32_C(2500);
    const uint32_t max_gap = ticks_per_us * UINT32_C(50000);
    return width1 && width1 <= max_width && gap && gap <= max_gap &&
           width2 && width2 <= max_width;
}

static void clear_frame(AM13E_PB14_Decoder *d)
{
    d->bits = 0U;
    d->decoded_bits = 0U;
    d->last_bit_period = 0U;
}

void am13e_pb14_decoder_abort(AM13E_PB14_Decoder *d)
{
    if (d == NULL) return;
    ++d->rejected;
    d->active = 0U;
    d->last_start = 0U;
    d->last_end = 0U;
    d->pending_width = 0U;
    clear_frame(d);
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
    if (d->decoded_bits == 16U &&
        d->rx_mode!=AM13E_PB14_RX_PWM &&
        d->rx_mode!=AM13E_PB14_RX_ONESHOT &&
        cb != NULL && cb(d->bits, d->inverted)) {
        ++d->good_dshot;
        d->rx_mode=AM13E_PB14_RX_DSHOT;
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

    /* Rel17 servoirq() runs at one MCU-timer tick per microsecond
     * for Servo or 8MHz/125ns for Oneshot125. Both give setthrot()
     * comparable 800..2200 logical pulse counts. To preserve that
     * source-level semantic, normalize a measured Oneshot pulse from
     * physical microseconds to Rel17's eight-count-per-us timer domain.
     * This is NOT the 'PWM_ENABLE' motor output option.
     */
    const int servo_candidate=
        period>=2500U*us_ticks && period<=50000U*us_ticks &&
        d->pending_width>=800U*us_ticks &&
        d->pending_width<=2200U*us_ticks;
    const int oneshot_candidate=
        period>=250U*us_ticks && period<=50000U*us_ticks &&
        d->pending_width>=100U*us_ticks &&
        d->pending_width<=275U*us_ticks &&
        d->pending_width<period;
    if(servo_candidate || oneshot_candidate) {
        const AM13E_PB14_RxMode kind=servo_candidate?
            AM13E_PB14_RX_PWM:AM13E_PB14_RX_ONESHOT;
        clear_frame(d);
        if(d->rx_mode!=AM13E_PB14_RX_UNDECIDED &&
           d->rx_mode!=kind) {
            ++d->rejected; /* Once selected, upstream servoirq
                              * and dshotirq do not auto-reselect. */
        } else {
            const unsigned pulse_us=(unsigned)(
                (d->pending_width+us_ticks/2U)/us_ticks);
            const unsigned source_ticks=servo_candidate?
                pulse_us:pulse_us*8U;
            if(pwm!=NULL) pwm(source_ticks);
            ++d->good_pwm;
            d->rx_mode=(uint8_t)kind;
        }
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
    /* The 16th falling edge finishes a valid DShot frame. Do not wait
     * for 16kHz SysTick: its 62.5us period misses the original Rel17
     * ~30us BiDShot turnaround budget. Last bit uses previous period.
     * CRC validation remains in the original src/io.c callback.
     */
    if (d->decoded_bits == 15U && d->last_bit_period != 0U) {
        finish_frame(d, dshot);
        d->active = 0U;
    }
}

void am13e_pb14_decoder_idle(AM13E_PB14_Decoder *d, uint32_t now,
                             AM13E_PB14_DshotCallback dshot)
{
    if (d==NULL || !d->tick_hz) return;
    /* Original rel17 switches from entryirq() to servoirq() or
     * dshotirq() and does NOT automatically reenter entryirq() after
     * 50ms of silence. Preserve that selected decoder mode.
     * The same 50ms existing capture-gap bound only quarantines
     * stale partial bits; a new mode requires an explicit reset.
     */
    if((d->last_start || d->last_end) &&
       (uint32_t)(now-d->last_end)>d->tick_hz/20U) {
        d->active=0U;
        d->last_start=0U;
        d->last_end=0U;
        d->pending_width=0U;
        clear_frame(d);
        return;
    }
    if (!d->active || !d->last_bit_period) return;
    /* Gap timeout remains a recovery path for truncated frames; a
     * normal 16-pulse DShot command finishes at its final captured edge.
     * Physical bidirectional output scheduling is a separate driver.
     */
    if ((uint32_t)(now - d->last_end) > d->last_bit_period * 4U) {
        finish_frame(d, dshot);
        d->active = 0U;
    }
}
