#include "aux_gpio.h"
#include "board_configuration.h"
#include <soc.h>
#include <dl_gpio.h>
#include <hw_pinmap.h>

#if defined(PARK_PIN) || defined(ERPM_PIN)
/* Conservative exclusions for every existing Reference owner, plus
 * explicit auxiliary/serial reservations. A new board must reconcile
 * resource ownership rather than selecting a conflicting spare pad.
 */
#define FREE_PIN(p) ((p)>0U && (p)<96U && \
    (p)!=IOMUX_PINCM_PC16_X1 && (p)!=IOMUX_PINCM_PC17_X2 && \
    (p)!=IOMUX_PINCM_PB13 && (p)!=IOMUX_PINCM_PB14 && (p)!=IOMUX_PINCM_PB15 && \
    (p)!=IOMUX_PINCM_PA8 && (p)!=IOMUX_PINCM_PA11 && (p)!=IOMUX_PINCM_PA9 && \
    (p)!=IOMUX_PINCM_PA30 && (p)!=IOMUX_PINCM_PA10 && (p)!=IOMUX_PINCM_PA31 && \
    (p)!=IOMUX_PINCM_PA6 && (p)!=IOMUX_PINCM_PA28 && (p)!=IOMUX_PINCM_PA17 && \
    (p)!=IOMUX_PINCM_PA4 && (p)!=IOMUX_PINCM_PA3 && (p)!=IOMUX_PINCM_PA2 && \
    (p)!=IOMUX_PINCM_PA16 && (p)!=IOMUX_PINCM_PA18 && \
    (p)!=AM13E_BOARD_OC_GPIO_PINCM && (p)!=AM13E_BOARD_SERIAL_TX_PINCM && \
    (p)!=AM13E_BOARD_CURRENT_SENSE_PINCM)
#define SINGLE_PIN(mask) ((mask)!=0U && ((mask)&((mask)-1U))==0U)
_Static_assert(IOMUX_PINCM_PA0==0U && IOMUX_PINCM_PB0==32U &&
               IOMUX_PINCM_PB31==63U && IOMUX_PINCM_PC0==64U &&
               IOMUX_PINCM_PC31==95U,"SDK GPIO/PINCM indexing changed");
static unsigned initialized;
static void fail(void) { __disable_irq(); for(;;){__NOP();} }
static int available(GPIO_Regs *gpio,uint32_t mask,unsigned pin)
{
    if(!FREE_PIN(pin))return 0;
    GPIO_Regs *expected=pin<32U?GPIO0:pin<64U?GPIO1:GPIO2;
    if(gpio!=expected || mask!=(UINT32_C(1)<<(pin&31U)))return 0;
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
    if(pin==AM13E_BOARD_SERIAL_RX_PINCM)return 0;
#endif
#ifdef AM13E_BOARD_TELEM_UART_CONFIGURED
    if(pin==AM13E_BOARD_TELEM_TX_PINCM)return 0;
#ifdef AM13E_BOARD_TELEM_RX_PINCM
    if(pin==AM13E_BOARD_TELEM_RX_PINCM)return 0;
#endif
#endif
    return 1;
}
#ifdef PARK_PIN
#if !defined(AM13E_BOARD_PARK_GPIO) || !defined(AM13E_BOARD_PARK_MASK) || \
    !defined(AM13E_BOARD_PARK_PINCM)
#error "PARK_PIN needs explicit AM13E GPIO/mask/PINCM board routing"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_PARK_PINCM) &&
               SINGLE_PIN(AM13E_BOARD_PARK_MASK) &&
               AM13E_BOARD_PARK_MASK==(UINT32_C(1)<<(AM13E_BOARD_PARK_PINCM&31U)),
               "PARK route conflicts or mask is invalid");
#endif
#ifdef ERPM_PIN
#if !defined(AM13E_BOARD_ERPM_GPIO) || !defined(AM13E_BOARD_ERPM_MASK) || \
    !defined(AM13E_BOARD_ERPM_PINCM)
#error "ERPM_PIN needs explicit AM13E GPIO/mask/PINCM board routing"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_ERPM_PINCM) &&
               SINGLE_PIN(AM13E_BOARD_ERPM_MASK) &&
               AM13E_BOARD_ERPM_MASK==(UINT32_C(1)<<(AM13E_BOARD_ERPM_PINCM&31U)),
               "ERPM route conflicts or mask is invalid");
#endif
#if defined(PARK_PIN) && defined(ERPM_PIN)
_Static_assert(AM13E_BOARD_PARK_PINCM!=AM13E_BOARD_ERPM_PINCM,
               "PARK and ERPM cannot share a pin");
#endif
#endif

void am13e_app_aux_gpio_init(void)
{
#if defined(PARK_PIN) || defined(ERPM_PIN)
    if(initialized || __get_PRIMASK()!=1U)fail();
#ifdef PARK_PIN
    if(!available(AM13E_BOARD_PARK_GPIO,AM13E_BOARD_PARK_MASK,
                  AM13E_BOARD_PARK_PINCM))fail();
    DL_GPIO_enablePower(AM13E_BOARD_PARK_GPIO);
    if(!DL_GPIO_isPowerEnabled(AM13E_BOARD_PARK_GPIO))fail();
    DL_GPIO_disableOutput(AM13E_BOARD_PARK_GPIO,AM13E_BOARD_PARK_MASK);
    DL_GPIO_initDigitalInputFeatures(AM13E_BOARD_PARK_PINCM,
        DL_GPIO_INVERSION_DISABLE,DL_GPIO_RESISTOR_PULL_UP,DL_GPIO_WAKEUP_DISABLE);
    if(!DL_GPIO_isInputEnabled(AM13E_BOARD_PARK_PINCM) ||
       DL_GPIO_getPeripheralFunctionBits(AM13E_BOARD_PARK_PINCM)!=1U)fail();
#endif
#ifdef ERPM_PIN
    if(!available(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK,
                  AM13E_BOARD_ERPM_PINCM))fail();
    DL_GPIO_enablePower(AM13E_BOARD_ERPM_GPIO);
    if(!DL_GPIO_isPowerEnabled(AM13E_BOARD_ERPM_GPIO))fail();
    DL_GPIO_disableOutput(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK);
    DL_GPIO_clearPins(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK);
    DL_GPIO_initDigitalOutput(AM13E_BOARD_ERPM_PINCM);
    if(DL_GPIO_getPeripheralFunctionBits(AM13E_BOARD_ERPM_PINCM)!=1U)fail();
    DL_GPIO_enableOutput(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK);
#endif
    initialized=1U;
#endif
}
#ifdef PARK_PIN
int am13e_app_park_level(void)
{
    if(!initialized)fail();
    return DL_GPIO_readPins(AM13E_BOARD_PARK_GPIO,AM13E_BOARD_PARK_MASK)!=0U;
}
#endif
#ifdef ERPM_PIN
void am13e_app_erpm_write(int high)
{
    if(!initialized)fail();
    if(high)DL_GPIO_setPins(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK);
    else DL_GPIO_clearPins(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK);
}
#endif
