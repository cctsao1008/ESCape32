#include "motor_timer_math.h"
#include <stdint.h>

uint32_t am13e_motor_us_to_timer_ticks(uint32_t us, uint32_t timer_hz)
{
    if (us == 0U || timer_hz == 0U) return 0U;
    const uint64_t ticks = ((uint64_t)us * timer_hz + UINT64_C(500000)) /
                           UINT64_C(1000000);
    if (ticks == 0U || ticks > UINT32_MAX) return 0U;
    return (uint32_t)ticks;
}
