/* Portable, bounded microsecond->peripheral timer tick conversion. */
#pragma once
#include <stdint.h>
/* Rounded to nearest count, returns 0 on zero/overflow/invalid timebase. */
uint32_t am13e_motor_us_to_timer_ticks(uint32_t us, uint32_t timer_hz);
