#include "service_reset.h"
#include "motor_backend.h"
#include <soc.h>

void am13e_app_service_reset(void)
{
    /* Cancel commutation/BEMF, force the gate inactive, freeze PWM and
     * disconnect pads before system reset. Physical gate qualification
     * and fault recovery authorization remain separate prerequisites.
     */
    am13e_app_motor_fault_shutdown();
    __DSB();
    NVIC_SystemReset();
    for (;;) { __NOP(); }
}
