/* AM13E development command input: PA26 -> TIMG4 CCP0 -> DMA2.
 * Bidirectional reply is implemented by the separate TIMG4/DMA backend.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E-only PA26 input driver"
#endif
#include <stdint.h>
#include <stdbool.h>

void initio(void); /* Original Rel17 entry, real MCU initialization. */
void am13e_pb14_resume_rx(void);
void am13e_app_pb14_systick(void); /* 16 kHz clock reference & timeout */
uint32_t am13e_pa26_rx_now(void);
void am13e_pa26_rx_suspend(void);
void am13e_pa26_rx_timer_irq(void);
void am13e_pa26_rx_dma_irq(void);
/* Masked-IRQ, idle-only handover from undecided capture to service CLI. */
bool am13e_app_pb14_service_takeover(void);

typedef struct {
    uint32_t capture_pairs;
    uint32_t capture_overruns; /* observed capture/DMA backlog */
    uint32_t late_capture_groups; /* reserved historical diagnostic */
    uint32_t max_capture_age_ticks; /* observed hardware timestamp age */
    uint32_t invalid_capture_groups; /* physical ordering/width rejected */
    uint32_t good_pwm;
    uint32_t good_dshot_rx;
    uint32_t watchdog_armed; /* WWDT0 started at receiver selection */
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
