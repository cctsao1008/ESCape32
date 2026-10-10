/* AM13E reference AM13E23019 ADC0 raw monitoring interface (Rel17 FW1 backend).
 * Raw codes ONLY: R25/Beta, VBUS divider, reference and calibration
 * are not defined in AM13E reference HW Architecture Baseline v1.6.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E ADC backend must not be used by legacy targets"
#endif
#include <stdint.h>

typedef struct {
    uint16_t ntc_main_adc0_in17; /* PA6, 12-bit raw ADC code */
    uint16_t vbus_adc0_in11;     /* PA28, 12-bit raw ADC code */
    uint32_t sample_count;
    uint32_t trigger_overruns;
} AM13E_AdcRaw;

/* Rel17's original main/housekeeping owns scheduling. */
void am13e_app_adc_init(void);
/* The Rel17 adctrig() declaration belongs to src/common.h. */
/* Strong TI CMSIS ADC0 interrupt handler (also declared for -Wmissing-prototypes). */
void ADC0_INT1_IRQHandler(void);

/* Debug/monitoring access. Returns 0 until first complete 2-channel scan.
 * Reads are coherent against ADC0 INT1 updates via the snapshot sequence.
 */
int am13e_app_adc_get_raw(AM13E_AdcRaw *out);
