#pragma once
#include <stdbool.h>
#include <stdint.h>
/* elapsed_ticks must be a qualified zero-cross interval in TIMG12 ticks. */
bool fw1_bemf_event_setup(void (*commutate)(void), uint32_t interval_ticks,
                          uint32_t electrical_time, unsigned timing);
bool fw1_bemf_event_capture_interval(uint32_t elapsed_ticks);
void fw1_bemf_event_timg12_irq(void);
void fw1_bemf_event_timeout(unsigned timer_xres);

/* Hardware entry: eCAP0 CAP1, when interval epoch/clock is configured. */
bool fw1_bemf_event_ecap0_event1(void);

/* Configure eCAP0 after peripheral clock/power configuration. */
#include "dl_ecap.h"
void fw1_bemf_ecap_start_epoch(void);
void fw1_bemf_ecap_configure_capture(DL_ECAP_INPUT source,
                                     DL_ECAP_EVENT_POLARITY edge);
