/* E62 FW1 PB14 command input: GPIO46 -> INPUTXBAR1 -> ECAP0.
 * Does not enable PB14 output or BiDShot telemetry TX.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only PB14 input driver"
#endif
#include <stdint.h>

void initio(void); /* Original Rel17 entry, real MCU initialization. */
void am13e_app_pb14_systick(void); /* 16 kHz reference & RX-only end-of-frame */
void ECAP0_IRQHandler(void);

typedef struct {
    uint32_t capture_pairs;
    uint32_t capture_overruns; /* observed phase movement during CAP1..CAP4 read */
    uint32_t good_pwm;
    uint32_t good_dshot_rx;
    uint32_t rejected;
    uint32_t unexpected_gpio1_irqs;
    uint32_t capture_ticks_per_us;
    uint8_t inverted_rx;
    uint8_t initialized;
} AM13E_PB14_Status;

/* Snapshot only. No TX mode is exposed until external I/F is qualified. */
void am13e_app_pb14_status(AM13E_PB14_Status *out);
