/* Rel17 six-step phase decoding contract, independent of MCU registers.
 * This does not implement physical MCPWM action qualifier/enable.
 */
#pragma once
#include <stdint.h>
typedef enum {
    AM13E_PHASE_FLOATING = 0,
    AM13E_PHASE_POSITIVE_PWM = 1,
    AM13E_PHASE_NEGATIVE_SINK = 2
} AM13E_PhaseRole;
typedef struct {
    AM13E_PhaseRole phase[3]; /* bit 0 U/A, 1 V/B, 2 W/C */
    uint8_t comparator_code; /* Rel17 compctl() encoded selection, NOT a mask */
    uint8_t damp;            /* Preserve complementary/active-freewheel choice */
    uint8_t reverse;
} AM13E_SixstepPlan;
/* Takes UNMODIFIED Rel17 p/n masks (including their upper encoded bits).
 * Returns 1 for a valid original Rel17 pair/selection, else 0.
 * p=n=0 is Rel17 zero-throttle coasting; no phase drive.
 */
int am13e_motor_plan_sixstep(int positive_mask, int negative_mask,
                            int comparator_code, int damp, int reverse,
                            AM13E_SixstepPlan *out);
