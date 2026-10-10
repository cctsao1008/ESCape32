/* ESCape32 Rel17 AM13E PB14 capture decoder (no TI register ownership).
 * Input timestamps come from successive hardware ECAP0 capture events.
 * DShot output generation is intentionally NOT part of this RX decoder.
 */
#pragma once
#include <stdint.h>

typedef enum {
    AM13E_PB14_RX_UNDECIDED=0,
    AM13E_PB14_RX_PWM=1,
    AM13E_PB14_RX_DSHOT=2
} AM13E_PB14_RxMode;
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
    uint8_t rx_mode; /* One logical mode at a time. */
} AM13E_PB14_Decoder;

/* Conservative four-edge capture sanity check. Uses calibrated eCAP Hz.
 * Recognizes rollover; rejects impossible pulse widths or edge ordering.
 * Returns 0 on uncalibrated clock or invalid group.
 */
/* A CEVT4 batch is only coherent when all four event flags were
 * latched, CAP slot wraps to the first event before AND after copying
 * CAP1..4, and no new capture event occurred after clearing the old
 * flags. Values/bitmasks are supplied by the real ECAP DriverLib
 * adapter; this function owns no TI registers or board pins.
 *
 * A physical complete 4-edge overwrite cannot be proved absent by
 * one ECAP register snapshot; measuring DShot600 ISR/DMA latency on
 * silicon remains a separate requirement.
 */
int am13e_pb14_capture_snapshot_valid(
    uint32_t flags_before, uint32_t flags_after,
    uint32_t required_events, unsigned phase_before,
    unsigned phase_after, unsigned next_expected_phase);

/* Conservative DShot ECAP group freshness admission. Two DShot bit
 * periods make one four-event batch; if the CPU reads the group after
 * the next CEVT4 deadline, the old registers cannot be trusted.
 * Servo PWM (>9us spacing) is not subject to this DShot-only bound.
 * True worst-case IRQ execution time still requires silicon evidence.
 */
int am13e_pb14_capture_budget_ok(uint32_t first_start,
                                  uint32_t second_start,
                                  uint32_t final_edge,
                                  uint32_t current_counter,
                                  uint32_t capture_hz);

int am13e_pb14_capture_group_valid(uint32_t start1, uint32_t end1,
                                   uint32_t start2, uint32_t end2,
                                   uint32_t tick_hz);

/* Protocol dispatch owns the original ESCape32 policy, not this decoder. */
typedef void (*AM13E_PB14_PwmCallback)(unsigned int pulse_us);
typedef int (*AM13E_PB14_DshotCallback)(uint16_t frame, int inverted);

void am13e_pb14_decoder_reset(AM13E_PB14_Decoder *d, uint32_t tick_hz, int inverted);
void am13e_pb14_decoder_pulse(AM13E_PB14_Decoder *d, uint32_t start,
                              uint32_t end, AM13E_PB14_PwmCallback pwm,
                              AM13E_PB14_DshotCallback dshot);
/* Drop a suspect capture group without retaining a partial DShot frame.
 * Preserve raw clock calibration and diagnostic totals.
 */
void am13e_pb14_decoder_abort(AM13E_PB14_Decoder *d);

void am13e_pb14_decoder_idle(AM13E_PB14_Decoder *d, uint32_t now,
                             AM13E_PB14_DshotCallback dshot);
