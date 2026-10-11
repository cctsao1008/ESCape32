#include "../aux_gpio.h"
#include "../board_configuration.h"
#include <soc.h>
#include <dl_gpio.h>
#include <hw_pinmap.h>
/* Preserve the original LED_MAP/LED_WS2812 count/feature selectors. */
#include "../../../src/defs.h"
#if LED_CNT > 0
#include "common.h"
#endif

#if defined(PARK_PIN) || defined(ERPM_PIN) || defined(BEC_MAP) || LED_CNT > 0
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
#if defined(PARK_PIN) || defined(ERPM_PIN)
static unsigned initialized;
#endif
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
#ifdef ANALOG_CHAN
    if(pin==AM13E_BOARD_ANALOG_INPUT_PINCM)return 0;
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


#if defined(BEC_MAP) && (LED_CNT > 0 || defined(PARK_PIN) || defined(ERPM_PIN))
static int bec_owns_pin(unsigned pin);
#endif
#if LED_CNT > 0
#ifdef LED_WS2812
#error "AM13E WS2812 timed peripheral backend remains unimplemented"
#endif
/* Discrete LEDs use original logical bits/polarity/open-drain selection. No default
 * product pads, voltage, current, external resistor or drive qualification. */
#if LED_CNT >= 1
#if !defined(AM13E_BOARD_LED1_GPIO) || !defined(AM13E_BOARD_LED1_MASK) || !defined(AM13E_BOARD_LED1_PINCM)
#error "LED_MAP needs explicit AM13E GPIO/mask/PINCM for each selected LED"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_LED1_PINCM) &&
    SINGLE_PIN(AM13E_BOARD_LED1_MASK) &&
    AM13E_BOARD_LED1_MASK==(UINT32_C(1)<<(AM13E_BOARD_LED1_PINCM&31U)),
    "LED route conflicts or mask is invalid");
#if defined(LED1_INV) || defined(LED_INV)
#define LED1_INVERTED 1U
#else
#define LED1_INVERTED 0U
#endif
#endif
#if LED_CNT >= 2
#if !defined(AM13E_BOARD_LED2_GPIO) || !defined(AM13E_BOARD_LED2_MASK) || !defined(AM13E_BOARD_LED2_PINCM)
#error "LED_MAP needs explicit AM13E GPIO/mask/PINCM for each selected LED"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_LED2_PINCM) &&
    SINGLE_PIN(AM13E_BOARD_LED2_MASK) &&
    AM13E_BOARD_LED2_MASK==(UINT32_C(1)<<(AM13E_BOARD_LED2_PINCM&31U)),
    "LED route conflicts or mask is invalid");
#if defined(LED2_INV) || defined(LED_INV)
#define LED2_INVERTED 1U
#else
#define LED2_INVERTED 0U
#endif
#endif
#if LED_CNT >= 3
#if !defined(AM13E_BOARD_LED3_GPIO) || !defined(AM13E_BOARD_LED3_MASK) || !defined(AM13E_BOARD_LED3_PINCM)
#error "LED_MAP needs explicit AM13E GPIO/mask/PINCM for each selected LED"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_LED3_PINCM) &&
    SINGLE_PIN(AM13E_BOARD_LED3_MASK) &&
    AM13E_BOARD_LED3_MASK==(UINT32_C(1)<<(AM13E_BOARD_LED3_PINCM&31U)),
    "LED route conflicts or mask is invalid");
#if defined(LED3_INV) || defined(LED_INV)
#define LED3_INVERTED 1U
#else
#define LED3_INVERTED 0U
#endif
#endif
#if LED_CNT >= 4
#if !defined(AM13E_BOARD_LED4_GPIO) || !defined(AM13E_BOARD_LED4_MASK) || !defined(AM13E_BOARD_LED4_PINCM)
#error "LED_MAP needs explicit AM13E GPIO/mask/PINCM for each selected LED"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_LED4_PINCM) &&
    SINGLE_PIN(AM13E_BOARD_LED4_MASK) &&
    AM13E_BOARD_LED4_MASK==(UINT32_C(1)<<(AM13E_BOARD_LED4_PINCM&31U)),
    "LED route conflicts or mask is invalid");
#if defined(LED4_INV)
#define LED4_INVERTED 1U
#else
#define LED4_INVERTED 0U
#endif
#endif
#if LED_CNT >= 2
_Static_assert(AM13E_BOARD_LED2_PINCM!=AM13E_BOARD_LED1_PINCM,"LED routes cannot share a pin");
#endif
#if LED_CNT >= 3
_Static_assert(AM13E_BOARD_LED3_PINCM!=AM13E_BOARD_LED1_PINCM,"LED routes cannot share a pin");
_Static_assert(AM13E_BOARD_LED3_PINCM!=AM13E_BOARD_LED2_PINCM,"LED routes cannot share a pin");
#endif
#if LED_CNT >= 4
_Static_assert(AM13E_BOARD_LED4_PINCM!=AM13E_BOARD_LED1_PINCM,"LED routes cannot share a pin");
_Static_assert(AM13E_BOARD_LED4_PINCM!=AM13E_BOARD_LED2_PINCM,"LED routes cannot share a pin");
_Static_assert(AM13E_BOARD_LED4_PINCM!=AM13E_BOARD_LED3_PINCM,"LED routes cannot share a pin");
#endif
typedef struct {
    GPIO_Regs *gpio;
    uint32_t mask;
    unsigned pin, inverted, open_drain;
} LedRoute;
/* Global LED_OD, like upstream LED_INV, aliases only LEDs1..3. */
#if defined(LED1_OD) || defined(LED_OD)
#define LED1_DRAIN 1U
#else
#define LED1_DRAIN 0U
#endif
#if defined(LED2_OD) || defined(LED_OD)
#define LED2_DRAIN 1U
#else
#define LED2_DRAIN 0U
#endif
#if defined(LED3_OD) || defined(LED_OD)
#define LED3_DRAIN 1U
#else
#define LED3_DRAIN 0U
#endif
#ifdef LED4_OD
#define LED4_DRAIN 1U
#else
#define LED4_DRAIN 0U
#endif
static const LedRoute leds[]={
#if LED_CNT >= 1
    {AM13E_BOARD_LED1_GPIO,AM13E_BOARD_LED1_MASK,AM13E_BOARD_LED1_PINCM,LED1_INVERTED,LED1_DRAIN},
#endif
#if LED_CNT >= 2
    {AM13E_BOARD_LED2_GPIO,AM13E_BOARD_LED2_MASK,AM13E_BOARD_LED2_PINCM,LED2_INVERTED,LED2_DRAIN},
#endif
#if LED_CNT >= 3
    {AM13E_BOARD_LED3_GPIO,AM13E_BOARD_LED3_MASK,AM13E_BOARD_LED3_PINCM,LED3_INVERTED,LED3_DRAIN},
#endif
#if LED_CNT >= 4
    {AM13E_BOARD_LED4_GPIO,AM13E_BOARD_LED4_MASK,AM13E_BOARD_LED4_PINCM,LED4_INVERTED,LED4_DRAIN},
#endif
};
_Static_assert(sizeof leds/sizeof leds[0]==LED_CNT,"LED count differs from source");
static unsigned led_initialized;
#if defined(PARK_PIN) || defined(ERPM_PIN) || defined(BEC_MAP)
static int led_owns_pin(unsigned pin)
{
    for(unsigned i=0U;i<LED_CNT;++i)if(leds[i].pin==pin)return 1;
    return 0;
}
#endif
void initled(void)
{
    if(led_initialized || __get_PRIMASK()!=1U)fail();
    /* Validate ALL owners before touching any output. */
    for(unsigned i=0U;i<LED_CNT;++i){
        const LedRoute *r=&leds[i];
        if(!available(r->gpio,r->mask,r->pin))fail();
#ifdef BEC_MAP
        if(bec_owns_pin(r->pin))fail();
#endif
#ifdef PARK_PIN
        if(r->pin==AM13E_BOARD_PARK_PINCM)fail();
#endif
#ifdef ERPM_PIN
        if(r->pin==AM13E_BOARD_ERPM_PINCM)fail();
#endif
    }
    for(unsigned i=0U;i<LED_CNT;++i){
        const LedRoute *r=&leds[i];
        DL_GPIO_enablePower(r->gpio);
        if(!DL_GPIO_isPowerEnabled(r->gpio))fail();
        DL_GPIO_disableOutput(r->gpio,r->mask);
        /* Set inactive latch while disconnected, including inverted LEDs. */
        if(r->inverted)DL_GPIO_setPins(r->gpio,r->mask);
        else DL_GPIO_clearPins(r->gpio,r->mask);
        DL_GPIO_initDigitalOutput(r->pin);
        if(DL_GPIO_getPeripheralFunctionBits(r->pin)!=1U)fail();
        DL_GPIO_setOpenDrain(r->pin,r->open_drain ? DL_GPIO_HIZ_ENABLE : DL_GPIO_HIZ_DISABLE);
        if(DL_GPIO_isOpenDrainEnabled(r->pin)!=!!r->open_drain)fail();
    }
    for(unsigned i=0U;i<LED_CNT;++i)
        DL_GPIO_enableOutput(leds[i].gpio,leds[i].mask);
    led_initialized=1U;
}
void ledctl(int value)
{
    if(!led_initialized)fail();
    for(unsigned i=0U;i<LED_CNT;++i){
        const LedRoute *r=&leds[i];
        if(((unsigned)value>>i&1U)^r->inverted)DL_GPIO_setPins(r->gpio,r->mask);
        else DL_GPIO_clearPins(r->gpio,r->mask);
    }
}
#endif

#ifdef BEC_MAP
#if BEC_MAP == 0xADE
#error "BEC on debug pins requires native external-pulldown/SWD qualification; no automatic override"
#endif
#if !defined(AM13E_BOARD_BEC_BITS) || AM13E_BOARD_BEC_BITS < 1 || AM13E_BOARD_BEC_BITS > 3
#error "BEC_MAP needs an explicit native 1..3-bit board route"
#endif
_Static_assert(BEC_MAX >= BEC_MIN && BEC_MAX-BEC_MIN < (1U<<AM13E_BOARD_BEC_BITS),
               "BEC voltage choices exceed configured GPIO bits");
typedef struct { GPIO_Regs *gpio; uint32_t mask; unsigned pin; } BecRoute;
#if AM13E_BOARD_BEC_BITS >= 1
#if !defined(AM13E_BOARD_BEC1_GPIO) || !defined(AM13E_BOARD_BEC1_MASK) || !defined(AM13E_BOARD_BEC1_PINCM)
#error "BEC_MAP needs explicit AM13E GPIO/mask/PINCM for each bit"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_BEC1_PINCM) && SINGLE_PIN(AM13E_BOARD_BEC1_MASK) &&
 AM13E_BOARD_BEC1_MASK==(UINT32_C(1)<<(AM13E_BOARD_BEC1_PINCM&31U)),
 "BEC route conflicts or mask is invalid");
#endif
#if AM13E_BOARD_BEC_BITS >= 2
#if !defined(AM13E_BOARD_BEC2_GPIO) || !defined(AM13E_BOARD_BEC2_MASK) || !defined(AM13E_BOARD_BEC2_PINCM)
#error "BEC_MAP needs explicit AM13E GPIO/mask/PINCM for each bit"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_BEC2_PINCM) && SINGLE_PIN(AM13E_BOARD_BEC2_MASK) &&
 AM13E_BOARD_BEC2_MASK==(UINT32_C(1)<<(AM13E_BOARD_BEC2_PINCM&31U)),
 "BEC route conflicts or mask is invalid");
#endif
#if AM13E_BOARD_BEC_BITS >= 3
#if !defined(AM13E_BOARD_BEC3_GPIO) || !defined(AM13E_BOARD_BEC3_MASK) || !defined(AM13E_BOARD_BEC3_PINCM)
#error "BEC_MAP needs explicit AM13E GPIO/mask/PINCM for each bit"
#endif
_Static_assert(FREE_PIN(AM13E_BOARD_BEC3_PINCM) && SINGLE_PIN(AM13E_BOARD_BEC3_MASK) &&
 AM13E_BOARD_BEC3_MASK==(UINT32_C(1)<<(AM13E_BOARD_BEC3_PINCM&31U)),
 "BEC route conflicts or mask is invalid");
#endif
static const BecRoute bec_routes[]={
#if AM13E_BOARD_BEC_BITS >= 1
 {AM13E_BOARD_BEC1_GPIO,AM13E_BOARD_BEC1_MASK,AM13E_BOARD_BEC1_PINCM},
#endif
#if AM13E_BOARD_BEC_BITS >= 2
 {AM13E_BOARD_BEC2_GPIO,AM13E_BOARD_BEC2_MASK,AM13E_BOARD_BEC2_PINCM},
#endif
#if AM13E_BOARD_BEC_BITS >= 3
 {AM13E_BOARD_BEC3_GPIO,AM13E_BOARD_BEC3_MASK,AM13E_BOARD_BEC3_PINCM},
#endif
};
static unsigned bec_initialized;
#if LED_CNT > 0 || defined(PARK_PIN) || defined(ERPM_PIN)
static int bec_owns_pin(unsigned pin)
{
    for(unsigned i=0U;i<AM13E_BOARD_BEC_BITS;++i)if(bec_routes[i].pin==pin)return 1;
    return 0;
}
#endif
static void bec_init(void)
{
    const int bits=am13e_app_bec_selector();
    if(bec_initialized || __get_PRIMASK()!=1U || bits<0 ||
       bits>BEC_MAX-BEC_MIN)fail();
#ifdef PARK_PIN
    if(!available(AM13E_BOARD_PARK_GPIO,AM13E_BOARD_PARK_MASK,AM13E_BOARD_PARK_PINCM))fail();
#endif
#ifdef ERPM_PIN
    if(!available(AM13E_BOARD_ERPM_GPIO,AM13E_BOARD_ERPM_MASK,AM13E_BOARD_ERPM_PINCM))fail();
#endif
    /* Check every owner and bit before touching ANY BEC output. */
    for(unsigned i=0U;i<AM13E_BOARD_BEC_BITS;++i){
        const BecRoute *r=&bec_routes[i];
        if(!available(r->gpio,r->mask,r->pin))fail();
        for(unsigned j=0U;j<i;++j)if(r->pin==bec_routes[j].pin)fail();
#if LED_CNT > 0
        if(led_owns_pin(r->pin))fail();
#endif
#ifdef PARK_PIN
        if(r->pin==AM13E_BOARD_PARK_PINCM)fail();
#endif
#ifdef ERPM_PIN
        if(r->pin==AM13E_BOARD_ERPM_PINCM)fail();
#endif
    }
    for(unsigned i=0U;i<AM13E_BOARD_BEC_BITS;++i){
        const BecRoute *r=&bec_routes[i];
        DL_GPIO_enablePower(r->gpio);
        if(!DL_GPIO_isPowerEnabled(r->gpio))fail();
        DL_GPIO_disableOutput(r->gpio,r->mask);
        if(((unsigned)bits>>i)&1U)DL_GPIO_setPins(r->gpio,r->mask);
        else DL_GPIO_clearPins(r->gpio,r->mask);
        DL_GPIO_initDigitalOutput(r->pin);
        DL_GPIO_setOpenDrain(r->pin,DL_GPIO_HIZ_DISABLE);
        if(DL_GPIO_getPeripheralFunctionBits(r->pin)!=1U ||
           DL_GPIO_isOpenDrainEnabled(r->pin))fail();
    }
    for(unsigned i=0U;i<AM13E_BOARD_BEC_BITS;++i)
        DL_GPIO_enableOutput(bec_routes[i].gpio,bec_routes[i].mask);
    bec_initialized=1U;
}
#endif

void am13e_app_aux_gpio_init(void)
{
#ifdef BEC_MAP
    bec_init();
#endif
#if defined(PARK_PIN) || defined(ERPM_PIN)
    if(initialized || __get_PRIMASK()!=1U)fail();
#if LED_CNT > 0
#ifdef PARK_PIN
    if(led_owns_pin(AM13E_BOARD_PARK_PINCM))fail();
#endif
#ifdef ERPM_PIN
    if(led_owns_pin(AM13E_BOARD_ERPM_PINCM))fail();
#endif
#endif
#ifdef PARK_PIN
#ifdef BEC_MAP
    if(bec_owns_pin(AM13E_BOARD_PARK_PINCM))fail();
#endif
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
#ifdef BEC_MAP
    if(bec_owns_pin(AM13E_BOARD_ERPM_PINCM))fail();
#endif
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
