/* AM13E reference board auxiliary IO-only initialization.
 *
 * PB15 (driver nFAULT) is a GPIO INPUT only. It is NOT sampled by
 * motor safety logic, does NOT install GPIO interrupts, and is NOT
 * routed to MCPWM Trip. PB14/DShot remains separately owned by ECAP0.
 *
 * Optional Independent OC, Serial Telemetry TX and Current Sense pins
 * default to 0 (NOT ASSIGNED). Once explicitly assigned, only their
 * physical pin mode is initialized here; no hardware Trip, UART or
 * current-measurement/limiting functionality is implemented.
 */
#include <soc.h>
#include <dl_gpio.h>
#include "fault_input.h"
#include "board_configuration.h"

#define AM13E_NFAULT_GPIO GPIO1
#define AM13E_NFAULT_PIN DL_GPIO_PIN(15U)
#define AM13E_NFAULT_PINCM IOMUX_PINCM_PB15

_Static_assert(IOMUX_PINCM_PB15 == 47U &&
               IOMUX_PINCM_PB13 == 45U &&
               IOMUX_PINCM_PB14 == 46U,
               "Reference auxiliary IO map changed");

#define AM13E_RESERVED_IO_VALID(p) \
    ((p)==0 || ((p)>0 && (p)<64 && \
     (p)!=IOMUX_PINCM_PB13 && (p)!=IOMUX_PINCM_PB14 && \
     (p)!=IOMUX_PINCM_PB15 && (p)!=IOMUX_PINCM_PA8 && \
     (p)!=IOMUX_PINCM_PA11 && (p)!=IOMUX_PINCM_PA9 && \
     (p)!=IOMUX_PINCM_PA30 && (p)!=IOMUX_PINCM_PA10 && \
     (p)!=IOMUX_PINCM_PA31 && (p)!=IOMUX_PINCM_PA6 && \
     (p)!=IOMUX_PINCM_PA28 && (p)!=IOMUX_PINCM_PA17 && \
     (p)!=IOMUX_PINCM_PA4 && (p)!=IOMUX_PINCM_PA3 && \
     (p)!=IOMUX_PINCM_PA2 && (p)!=IOMUX_PINCM_PA16 && \
     (p)!=IOMUX_PINCM_PA18))
_Static_assert(AM13E_RESERVED_IO_VALID(AM13E_BOARD_OC_GPIO_PINCM) &&
               AM13E_RESERVED_IO_VALID(AM13E_BOARD_SERIAL_TX_PINCM) &&
               AM13E_RESERVED_IO_VALID(AM13E_BOARD_CURRENT_SENSE_PINCM),
               "Aux IO reservation conflicts with an AM13E reference pin");
_Static_assert((AM13E_BOARD_OC_GPIO_PINCM==0 ||
                AM13E_BOARD_OC_GPIO_PINCM!=AM13E_BOARD_SERIAL_TX_PINCM) &&
               (AM13E_BOARD_OC_GPIO_PINCM==0 ||
                AM13E_BOARD_OC_GPIO_PINCM!=AM13E_BOARD_CURRENT_SENSE_PINCM) &&
               (AM13E_BOARD_SERIAL_TX_PINCM==0 ||
                AM13E_BOARD_SERIAL_TX_PINCM!=AM13E_BOARD_CURRENT_SENSE_PINCM),
               "Aux IO reservations must use distinct pins");

static void io_init_fail(void)
{
    __disable_irq();
    for (;;) { __NOP(); }
}

void initgpio(void)
{
    /* Do not reset GPIO1: PB14 DShot shares the same port. */
    DL_GPIO_enablePower(AM13E_NFAULT_GPIO);
    if (!DL_GPIO_isPowerEnabled(AM13E_NFAULT_GPIO))
        io_init_fail();

    /* nFAULT: input ONLY. Keep PB15 interrupt disabled regardless of
     * its physical input level; no protection semantics are claimed.
     */
    DL_GPIO_disableOutput(AM13E_NFAULT_GPIO, AM13E_NFAULT_PIN);
    DL_GPIO_initDigitalInput(AM13E_NFAULT_PINCM);
    DL_GPIO_disableInterrupt(AM13E_NFAULT_GPIO, AM13E_NFAULT_PIN);
    DL_GPIO_clearInterruptStatus(AM13E_NFAULT_GPIO, AM13E_NFAULT_PIN);
    if (!DL_GPIO_isInputEnabled(AM13E_NFAULT_PINCM) ||
        DL_GPIO_getPeripheralFunctionBits(AM13E_NFAULT_PINCM)!=
            IOMUX_PB15_GPIO47 ||
        DL_GPIO_getEnabledInterrupts(AM13E_NFAULT_GPIO,
                                      AM13E_NFAULT_PIN)!=0U)
        io_init_fail();

#if AM13E_BOARD_OC_GPIO_PINCM != 0
    /* Dedicated OC: pin input only; NO INPUTXBAR/PWMXBAR/OST2. */
    DL_GPIO_initDigitalInput(AM13E_BOARD_OC_GPIO_PINCM);
    if (!DL_GPIO_isInputEnabled(AM13E_BOARD_OC_GPIO_PINCM))
        io_init_fail();
#endif
#if AM13E_BOARD_SERIAL_TX_PINCM != 0
    /* TX reservation: GPIO input (Hi-Z); NO UART mux/transmission. */
    DL_GPIO_initDigitalInput(AM13E_BOARD_SERIAL_TX_PINCM);
    if (!DL_GPIO_isInputEnabled(AM13E_BOARD_SERIAL_TX_PINCM))
        io_init_fail();
#endif
#if AM13E_BOARD_CURRENT_SENSE_PINCM != 0
    /* Current-sense reservation: analog pinmux only. No ADC SOC/ISR. */
    DL_GPIO_initPeripheralAnalogFunction(AM13E_BOARD_CURRENT_SENSE_PINCM);
#endif
}
