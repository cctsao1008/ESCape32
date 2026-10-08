/*
 * ESCape32 rel17 six-step commutation policy.
 * Independent of libopencm3, TI DriverLib and physical pin assignment.
 * Encoding is preserved verbatim from rel17 src/main.c.
 */
#pragma once
#include <stdint.h>

typedef struct {
    unsigned positive;
    unsigned negative;
    unsigned comparator;
} hw_six_step_t;

static inline hw_six_step_t hw_six_step_decode(unsigned step, unsigned reverse)
{
    static const uint16_t sequence[6] = {
        0x175U, 0xd9U, 0x1abU, 0x72U, 0x1deU, 0xacU
    };
    /* Caller contract: step is in [1,6]; not a physical pin-map lookup. */
    const unsigned x = sequence[step - 1U];
    const unsigned energized = x >> 3;
    const hw_six_step_t result = {
        .positive = x & energized,
        .negative = (~x) & energized,
        .comparator = (energized >> 3) ^ ((reverse != 0U) << 2)
    };
    return result;
}
