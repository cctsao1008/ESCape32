/* AM13E reference AM13E23019 three-ADC raw monitoring interface (Rel17 FW1 backend).
 * Raw codes ONLY: R25/Beta, VBUS divider, reference and calibration
 * are not defined in AM13E reference HW Architecture Baseline v1.6.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E ADC backend must not be used by legacy targets"
#endif
#include <stdint.h>
#include "analog_calibration_plan.h"

typedef struct {
    uint16_t current_u,current_v,current_w;
    uint16_t phase_a,phase_b,phase_c;
    uint16_t vbus,ibus,ntc;
    uint32_t sample_count,trigger_overruns;
    uint32_t acquisition_tick,completion_tick;
} AM13E_AdcRaw;

/* Rel17's original main/housekeeping owns scheduling. */
void am13e_app_adc_init(void);
/* The Rel17 adctrig() declaration belongs to src/common.h. */
/* Strong TI CMSIS ADC interrupt handlers (also declared for -Wmissing-prototypes). */
void ADC0_INT1_IRQHandler(void);
void ADC1_INT1_IRQHandler(void);
void ADC2_INT1_IRQHandler(void);

/* Debug/monitoring access. Returns 0 until first complete nine-code epoch.
 * Reads are coherent against all three ADC INT1 completions via the snapshot sequence.
 */
int am13e_app_adc_get_raw(AM13E_AdcRaw *out);
/* Bounded atomic pending-reference copy, suitable for a masked motor
 * transaction. It never reads an ADC-owned snapshot or calculates in ISR. */
int am13e_app_adc_get_reference(AM13E_BemfReference *out);
