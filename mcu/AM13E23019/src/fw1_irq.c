/* E62 FW1 interrupt registration via TI Cortex-M33 architecture layer.
 * Registering handlers relocates the interrupt vector to SRAM as required
 * by the TI SDK. Peripheral NVIC enable is separate from eCAP enable.
 */
#include <stdbool.h>
#include "arch_interrupt.h"
#include "hw/hw_ints.h"
#include "fw1_bemf_events.h"
#include "fw1_irq.h"

static void fw1_ecap0_irq(void)
{
    (void)fw1_bemf_event_ecap0_event1();
}
static void fw1_timg12_irq(void)
{
    fw1_bemf_event_timg12_irq();
}

bool fw1_register_bemf_irqs(void)
{
    if (Arch_Interrupt_RegisterHandler(ECAP0_INT_IRQn, fw1_ecap0_irq) != 0)
        return false;
    if (Arch_Interrupt_RegisterHandler(TIMG12_0_INT_IRQn, fw1_timg12_irq) != 0) {
        (void)Arch_Interrupt_UnRegisterHandler(ECAP0_INT_IRQn);
        return false;
    }
    if (Arch_Interrupt_Clear(ECAP0_INT_IRQn) != 0 ||
        Arch_Interrupt_Clear(TIMG12_0_INT_IRQn) != 0)
        return false;
    return true;
}

/* Call only after E62 CMPSS/eCAP source, edge, pinmux, epoch and clock
 * domain have been configured. Otherwise IRQs remain disabled.
 */
bool fw1_enable_bemf_irqs(void)
{
    if (Arch_Interrupt_Enable(TIMG12_0_INT_IRQn) != 0)
        return false;
    if (Arch_Interrupt_Enable(ECAP0_INT_IRQn) != 0) {
        (void)Arch_Interrupt_Disable(TIMG12_0_INT_IRQn);
        return false;
    }
    return true;
}
