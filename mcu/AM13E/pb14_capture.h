/* E62 FW1 PB14 command input: GPIO46 -> INPUTXBAR1 -> ECAP0.
 * Bidirectional reply is implemented by the separate TIMG4/DMA backend.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only PB14 input driver"
#endif
#include <stdint.h>

void initio(void); /* Original Rel17 entry, real MCU initialization. */
void am13e_pb14_resume_rx(void);
void am13e_app_pb14_systick(void); /* 16 kHz clock reference & timeout */
void ECAP0_IRQHandler(void);

typedef struct {
    uint32_t capture_pairs;
    uint32_t capture_overruns; /* observed phase movement during CAP1..CAP4 read */
    uint32_t invalid_capture_groups; /* physical ordering/width rejected */
    uint32_t good_pwm;
    uint32_t good_dshot_rx;
    uint32_t watchdog_armed; /* WWDT0 started after first valid receiver frame */
    uint32_t watchdog_valid_feeds; /* Valid PWM/DShot feed callback count */
    uint32_t tx_dma_completed; /* Software DMA completion IRQ count */
    uint32_t tx_rejected; /* Unsupported rate or missed TX deadline */
    uint32_t rejected;
    uint32_t unexpected_gpio1_irqs;
    uint32_t capture_ticks_per_us;
    uint8_t inverted_rx;
    uint8_t initialized;
} AM13E_PB14_Status;

/* Software snapshot only; pin waveform needs oscilloscope qualification. */
void am13e_app_pb14_status(AM13E_PB14_Status *out);
