/* E62 AM13E interrupt registration via TI Cortex-M33 architecture layer.
 * Registering handlers relocates the interrupt vector to SRAM as required
 * by the TI SDK. Peripheral NVIC enable is separate from eCAP enable.
 */
#include <stdbool.h>
#include "arch_interrupt.h"
#include "hw/hw_ints.h"
#include "am13e_rel17_bemf_io.h"
#include "am13e_irq.h"

static void am13e_ecap0_irq(void)
{
    (void)am13e_rel17_bemf_ecap0_irq();
}
static void am13e_timg12_irq(void)
{
    am13e_rel17_bemf_timg12_irq();
}

bool am13e_register_bemf_irqs(void)
{
    if (Arch_Interrupt_RegisterHandler(ECAP0_INT_IRQn, am13e_ecap0_irq) != 0)
        return false;
    if (Arch_Interrupt_RegisterHandler(TIMG12_0_INT_IRQn, am13e_timg12_irq) != 0) {
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
bool am13e_enable_bemf_irqs(void)
{
    if (Arch_Interrupt_Enable(TIMG12_0_INT_IRQn) != 0)
        return false;
    if (Arch_Interrupt_Enable(ECAP0_INT_IRQn) != 0) {
        (void)Arch_Interrupt_Disable(TIMG12_0_INT_IRQn);
        return false;
    }
    return true;
}
