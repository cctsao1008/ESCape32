/* Hardware-only BEMF event I/O for the authoritative ESCape32 motor-control loop.
 * This module owns NO interval/sync/fast/timing policy state.
 * Caller must validate eCAP epoch and eCAP/TIMG12 clock equivalence first.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "hw_am13e_safety_contract.h"

typedef void (*am13e_bemf_capture_callback_t)(uint32_t elapsed_ecap_ticks);
typedef void (*am13e_bemf_due_callback_t)(void);

bool am13e_bemf_bind(am13e_bemf_capture_callback_t capture,
                          am13e_bemf_due_callback_t due,
                     const am13e_motor_contract_t *contract);
/* Argument is in eCAP ticks; conversion to TIMG12 is explicit. */
bool am13e_bemf_arm_delay(uint32_t elapsed_ecap_ticks);
void am13e_bemf_cancel_delay(void);
void am13e_bemf_ecap0_irq(void);
void am13e_bemf_timg12_irq(void);
