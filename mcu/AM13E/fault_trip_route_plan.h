/* Pure AM13E INPUTXBAR/PWMXBAR/OST route policy (no hardware I/O).
 * Pin electrical suitability remains a selected board responsibility.
 */
#pragma once
#include <stdint.h>
int am13e_fault_trip_route_fields_valid(unsigned gpio_index,
                                         unsigned input_xbar,
                                         unsigned pwm_trip,
                                         unsigned pwm_source,
                                         uint32_t ost_signal,
                                         uint32_t ost_flag);
