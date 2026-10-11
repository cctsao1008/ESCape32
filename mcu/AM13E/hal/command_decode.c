/* Protocol-independent hardware timestamp validation for PB14 RX.
 * A complete DShot packet enters the existing Rel17 CRC/throttle callback.
 * This decoder does not fabricate a watchdog feed, telemetry or PWM output.
 */
#include "command_decode.h"
#include "receiver_entry.h"
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
    d->entry_mode = d->entry_captures = 0U;
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
        d->rx_mode==AM13E_PB14_RX_DSHOT &&
        cb != NULL && cb(d->bits, d->inverted)) {
        ++d->good_dshot;
        d->rx_mode=AM13E_PB14_RX_DSHOT;
    } else if (d->decoded_bits != 0U) {
        ++d->rejected;
    }
    clear_frame(d);
}

int am13e_pb14_decoder_bidir_reply_due(
    const AM13E_PB14_Decoder *d,
    uint32_t pulse15_start,uint32_t pulse16_start)
{
    if (d==NULL || !d->active || d->decoded_bits!=13U ||
        !d->last_bit_period || !d->tick_hz ||
        d->rx_mode!=AM13E_PB14_RX_DSHOT)
        return 0;
    const uint32_t p=pulse16_start-pulse15_start;
    const uint32_t gap=pulse15_start-d->last_start;
    const uint64_t old=d->last_bit_period;
    /* Confirm that the final two incoming bit periods belong to
     * the captured DShot timing epoch, independent of CRC value.
     */
    return p && rel17_receiver_dshot_rate((int)p,
                   (int)((d->tick_hz+400000U)/800000U))==d->dshot_rate &&
           (uint64_t)p*5U>=old*4U &&
           (uint64_t)p*5U<=old*6U &&
           (uint64_t)gap*5U>=old*4U &&
           (uint64_t)gap*5U<=old*6U;
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
    const int servo_candidate=period>=2000U*us_ticks && period<=50000U*us_ticks &&
        rel17_receiver_servo_period(period/us_ticks);
    const int oneshot_candidate=!servo_candidate && period>=250U*us_ticks &&
        period<=50000U*us_ticks &&
        rel17_receiver_servo_period((int)((uint64_t)period*8U/us_ticks));
    /* Once Rel17 selects servoirq, special and invalid widths belong
     * to that policy too. Do not silently filter them in the decoder.
     * Keep mode selection locked; never reinterpret DShot as Servo.
     */
    const int selected_servo=d->rx_mode==AM13E_PB14_RX_PWM &&
        period>9U*us_ticks && period<=50000U*us_ticks;
    const int selected_oneshot=d->rx_mode==AM13E_PB14_RX_ONESHOT &&
        period>9U*us_ticks && period<=50000U*us_ticks;
    if(servo_candidate || oneshot_candidate || selected_servo || selected_oneshot) {
        const AM13E_PB14_RxMode kind=selected_servo?AM13E_PB14_RX_PWM:
            selected_oneshot?AM13E_PB14_RX_ONESHOT:
            servo_candidate?AM13E_PB14_RX_PWM:AM13E_PB14_RX_ONESHOT;
        clear_frame(d);
        if(d->rx_mode!=AM13E_PB14_RX_UNDECIDED &&
           d->rx_mode!=kind) {
            ++d->rejected; /* Once selected, upstream servoirq
                              * and dshotirq do not auto-reselect. */
        } else {
            /* Preserve entryirq's first invalid capture and eight-capture
             * qualification. Width validation belongs to servoirq, even
             * when the selected frame contains an invalid/special width. */
            if(d->rx_mode==AM13E_PB14_RX_UNDECIDED) {
                if(d->entry_mode!=(uint8_t)kind) {
                    d->entry_mode=(uint8_t)kind;
                    d->entry_captures=2U;
                } else if(d->entry_captures<REL17_SERVO_ENTRY_CAPTURES) {
                    ++d->entry_captures;
                }
                if(d->entry_captures<REL17_SERVO_ENTRY_CAPTURES)
                    goto next_pulse;
            }
            const unsigned scale=kind==AM13E_PB14_RX_ONESHOT?8U:1U;
            const unsigned source_ticks=(unsigned)(
                ((uint64_t)d->pending_width*scale+us_ticks/2U)/us_ticks);
            const unsigned source_period=(unsigned)(
                ((uint64_t)period*scale+us_ticks/2U)/us_ticks);
            if(pwm!=NULL) pwm(source_ticks,source_period);
            ++d->good_pwm;
            d->rx_mode=(uint8_t)kind;
        }
    } else if (period >= us_ticks / 2U && period <= 9U * us_ticks &&
               d->pending_width < period &&
               (d->last_bit_period == 0U ||
                ((uint64_t)period * 5U >= (uint64_t)d->last_bit_period * 4U &&
                 (uint64_t)period * 5U <= (uint64_t)d->last_bit_period * 6U))) {
        const int rate=rel17_receiver_dshot_rate((int)period,
                            (int)((d->tick_hz+400000U)/800000U));
        if(rate<0 || (d->rx_mode==AM13E_PB14_RX_DSHOT &&
                     rate!=d->dshot_rate)) {
            ++d->rejected;
            d->entry_mode=d->entry_captures=0U;
            clear_frame(d);
            goto next_pulse;
        }
        if(d->rx_mode==AM13E_PB14_RX_UNDECIDED) {
            const uint8_t candidate=(uint8_t)(4+rate);
            /* DShot selection precedes CRC, as in entryirq(). Invalid
             * CRC must neither feed WWDT nor make CLI eligible again. */
            if(d->entry_mode!=candidate) {
                clear_frame(d);
                d->entry_mode=candidate;d->entry_captures=2U;
            } else if(d->entry_captures<REL17_DSHOT_ENTRY_CAPTURES) {
                ++d->entry_captures;
            }
            if(d->entry_captures>=REL17_DSHOT_ENTRY_CAPTURES) {
                d->rx_mode=AM13E_PB14_RX_DSHOT;
                d->dshot_rate=(uint8_t)rate;
            }
        }
        if (!bit_push(d, d->pending_width, period)) {
            ++d->rejected;
            clear_frame(d);
        }
    } else {
        /* DShot packet ended, or input timing changed. A frame has 16
         * pulses; accept nothing without a full frame and Rel17 CRC.
         */
        d->entry_mode=d->entry_captures=0U;
        if (d->decoded_bits) finish_frame(d, dshot);
        else clear_frame(d);
    }
next_pulse:
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
        d->entry_mode=d->entry_captures=0U;
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
