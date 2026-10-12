/* Optional Rel17 Hall/hybrid hardware seam. EQEP0 unit timer is an
 * independent 200 MHz timebase/deadline; no TIMG or ECAP owner changes.
 * GPIO edges timestamp ISR entry, not a hardware capture latch. Board
 * selection explicitly acknowledges that backend; latency/filtering must
 * be measured before electrical/timing qualification. No default Hall pads.
 */
#ifdef HALL_MAP
#include "motor_hall.h"
#include "motor_backend.h"
#include "clock_backend.h"
#include "board_configuration.h"
#include "board_motor_output_provider.h"
#include "../../../src/defs.h"
#include <dl_gpio.h>
#include <dl_eqep.h>
#include <dl_sysctl.h>
#include <stdint.h>

#if !defined(AM13E_HALL_GPIO_TIMESTAMP) || AM13E_HALL_GPIO_TIMESTAMP != 1 || \
    !defined(AM13E_HALL_PIN1) || !defined(AM13E_HALL_PIN2) || \
    !defined(AM13E_HALL_PIN3) || !defined(AM13E_HALL_PULL) || \
    !defined(AM13E_HALL_INVERT_MASK)
#error "Hall requires three explicit GPIO0 routes, pull/inversion and GPIO timestamp backend selection"
#endif
/* Native GPIO edge detection substitutes for TIM3's three-input XOR.
 * USE_XOR selects that same any-Hall-edge intent; it reserves no extra pin.
 */
#define FREE_HALL(p) ((p)>0U && (p)<32U && \
    (p)!=8U && (p)!=11U && (p)!=9U && (p)!=30U && (p)!=10U && (p)!=31U && \
    (p)!=6U && (p)!=28U && (p)!=17U && (p)!=4U && (p)!=3U && (p)!=2U && \
    (p)!=16U && (p)!=18U && (p)!=AM13E_BOARD_OC_GPIO_PINCM && \
    (p)!=AM13E_BOARD_SERIAL_TX_PINCM && (p)!=AM13E_BOARD_CURRENT_SENSE_PINCM)
_Static_assert(FREE_HALL(AM13E_HALL_PIN1) && FREE_HALL(AM13E_HALL_PIN2) &&
    FREE_HALL(AM13E_HALL_PIN3) && AM13E_HALL_PIN1!=AM13E_HALL_PIN2 &&
    AM13E_HALL_PIN1!=AM13E_HALL_PIN3 && AM13E_HALL_PIN2!=AM13E_HALL_PIN3,
    "Hall routes conflict with native motor/sensor/reference owners");
_Static_assert(AM13E_HALL_INVERT_MASK>=0 && AM13E_HALL_INVERT_MASK<8 &&
    (AM13E_HALL_PULL==DL_GPIO_RESISTOR_NONE ||
     AM13E_HALL_PULL==DL_GPIO_RESISTOR_PULL_UP ||
     AM13E_HALL_PULL==DL_GPIO_RESISTOR_PULL_DOWN),"Invalid Hall electrical definition");
#define CHECK_OWNER(p) _Static_assert((p)!=AM13E_HALL_PIN1 && \
    (p)!=AM13E_HALL_PIN2 && (p)!=AM13E_HALL_PIN3,"Hall pad already owned")
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
CHECK_OWNER(AM13E_BOARD_SERIAL_RX_PINCM);
#endif
#ifdef AM13E_BOARD_TELEM_UART_CONFIGURED
CHECK_OWNER(AM13E_BOARD_TELEM_TX_PINCM);
#ifdef AM13E_BOARD_TELEM_RX_PINCM
CHECK_OWNER(AM13E_BOARD_TELEM_RX_PINCM);
#endif
#endif
#ifdef ANALOG_CHAN
CHECK_OWNER(AM13E_BOARD_ANALOG_INPUT_PINCM);
#endif
#ifdef PARK_PIN
CHECK_OWNER(AM13E_BOARD_PARK_PINCM);
#endif
#ifdef ERPM_PIN
CHECK_OWNER(AM13E_BOARD_ERPM_PINCM);
#endif
/* Dynamic LED/BEC board providers need a shared route claim before their
 * GPIOs can coexist with this optional input owner. Never overwrite them. */
#if defined(BEC_MAP) || LED_CNT > 0
#error "Hall with LED/BEC providers requires shared pad ownership validation"
#endif
#define HALL_MASK (DL_GPIO_PIN(AM13E_HALL_PIN1)|DL_GPIO_PIN(AM13E_HALL_PIN2)|DL_GPIO_PIN(AM13E_HALL_PIN3))
#define HALL_FILTER (DL_GPIO_PIN_INPUT_FILTER_8_CYCLES(AM13E_HALL_PIN1)| \
    DL_GPIO_PIN_INPUT_FILTER_8_CYCLES(AM13E_HALL_PIN2)|DL_GPIO_PIN_INPUT_FILTER_8_CYCLES(AM13E_HALL_PIN3))
#define TIMEOUT_TICKS ((AM13E_APP_MCLK_HZ/1000000U)*32768U)
_Static_assert(AM13E_APP_MCLK_HZ==200000000U,"Hall EQEP0 MCLK contract changed");
static volatile unsigned initialized,active;
static void fail(void) { am13e_app_motor_fault_shutdown();am13e_app_motor_fault_reset(); }

int hallcode(void)
{
    const uint32_t x=DL_GPIO_readPins(GPIO0,HALL_MASK);
    return (((x>>AM13E_HALL_PIN1)&1U) |
            (((x>>AM13E_HALL_PIN2)&1U)<<1U) |
            (((x>>AM13E_HALL_PIN3)&1U)<<2U)) ^ AM13E_HALL_INVERT_MASK;
}
void am13e_app_motor_hall_gpio_init(void)
{
    if(__get_PRIMASK()!=1U)fail();
    DL_GPIO_enablePower(GPIO0);
    if(!DL_GPIO_isPowerEnabled(GPIO0))fail();
    if(DL_GPIO_getEnabledInterrupts(GPIO0,~HALL_MASK))fail();
    const unsigned pin[3]={AM13E_HALL_PIN1,AM13E_HALL_PIN2,AM13E_HALL_PIN3};
    const AM13E_MotorPadRoute *motor=am13e_board_motor_pad_route();
    for(unsigned i=0;i<3U;++i)for(unsigned j=0;j<AM13E_MOTOR_PAD_COUNT;++j)
        if(pin[i]==motor->pincm[j])fail();
    DL_GPIO_disableOutput(GPIO0,HALL_MASK);
    DL_GPIO_disableInterrupt(GPIO0,HALL_MASK);
    for(unsigned i=0;i<3U;++i){
        DL_GPIO_initDigitalInputFeatures(pin[i],DL_GPIO_INVERSION_DISABLE,
            AM13E_HALL_PULL,DL_GPIO_WAKEUP_DISABLE);
        if(!DL_GPIO_isInputEnabled(pin[i]) ||
           DL_GPIO_getPeripheralFunctionBits(pin[i])!=1U)fail();
    }
    const uint64_t edges=DL_GPIO_PIN_EDGE_RISE_FALL(AM13E_HALL_PIN1)|
        DL_GPIO_PIN_EDGE_RISE_FALL(AM13E_HALL_PIN2)|DL_GPIO_PIN_EDGE_RISE_FALL(AM13E_HALL_PIN3);
    DL_GPIO_setPinsPolarity(GPIO0,edges,edges);
    /* Native maximum eight-cycle filter. GPIO has no TIM3 DIV8/N8
     * capture filter; do not claim identical pulse rejection time. */
    DL_GPIO_setPinsInputFilter(GPIO0,HALL_FILTER,HALL_FILTER);
    DL_GPIO_clearInterruptStatus(GPIO0,HALL_MASK);
    initialized=1U;
}
void am13e_app_motor_hall_init(int enable)
{
    if(!initialized || __get_PRIMASK()!=1U || (enable!=0 && enable!=1))fail();
    active=0U;
    DL_GPIO_disableInterrupt(GPIO0,HALL_MASK);
    NVIC_DisableIRQ(GPIO0_INT_IRQn);
    NVIC_DisableIRQ(EQEP0_INT_IRQn);
    NVIC_ClearPendingIRQ(GPIO0_INT_IRQn);NVIC_ClearPendingIRQ(EQEP0_INT_IRQn);
    if(DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_EQEP0)){
        DL_EQEP_disableUnitTimer(EQEP0);
        DL_EQEP_disableInterrupt(EQEP0,UINT16_MAX);
        DL_EQEP_clearInterruptStatus(EQEP0,UINT16_MAX);
    }
    if(!enable)return; /* Rel17 brushed / code 7 sensorless selection. */
    DL_SYSCTL_enablePower(DL_SYSCTL_PWREN_EQEP0);
    if(!DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_EQEP0))fail();
    /* Use only the unit timer; decoder/capture/position outputs stay off. */
    EQEP0->QEPCTL=0U;EQEP0->QCAPCTL=0U;EQEP0->QPOSCTL=0U;
    EQEP0->QEINT=DL_EQEP_INT_UNIT_TIME_OUT;EQEP0->QUTMR=0U;
    DL_EQEP_clearInterruptStatus(EQEP0,UINT16_MAX);
    DL_EQEP_enableUnitTimer(EQEP0,TIMEOUT_TICKS);
    NVIC_SetPriority(GPIO0_INT_IRQn,0U);NVIC_SetPriority(EQEP0_INT_IRQn,0U);
    NVIC_ClearPendingIRQ(GPIO0_INT_IRQn);NVIC_ClearPendingIRQ(EQEP0_INT_IRQn);
    active=1U;
    DL_GPIO_clearInterruptStatus(GPIO0,HALL_MASK);
    DL_GPIO_enableInterrupt(GPIO0,HALL_MASK);
    NVIC_EnableIRQ(EQEP0_INT_IRQn);NVIC_EnableIRQ(GPIO0_INT_IRQn);
}
int am13e_app_motor_hall_ready(void)
{
    if(!initialized)return 0;
    const unsigned pin[3]={AM13E_HALL_PIN1,AM13E_HALL_PIN2,AM13E_HALL_PIN3};
    for(unsigned i=0;i<3U;++i)
        if(!DL_GPIO_isInputEnabled(pin[i]) ||
           DL_GPIO_getPeripheralFunctionBits(pin[i])!=1U)return 0;
    if((GPIO0->DOE31_0&HALL_MASK) || DL_GPIO_getEnabledInterrupts(GPIO0,~HALL_MASK))return 0;
    if((DL_GPIO_getPinsInputFilter(GPIO0)&HALL_FILTER)!=HALL_FILTER)return 0;
    if(!active)return DL_GPIO_getEnabledInterrupts(GPIO0,HALL_MASK)==0U;
    return NVIC_GetVector(GPIO0_INT_IRQn)==((uint32_t)(uintptr_t)GPIO0_IRQHandler|1U) &&
        NVIC_GetVector(EQEP0_INT_IRQn)==((uint32_t)(uintptr_t)EQEP0_IRQHandler|1U) &&
        NVIC_GetPriority(GPIO0_INT_IRQn)==0U && NVIC_GetPriority(EQEP0_INT_IRQn)==0U &&
        DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_EQEP0) &&
        EQEP0->QUPRD==TIMEOUT_TICKS && (EQEP0->QEPCTL&EQEP_QEPCTL_UTE_MASK) &&
        EQEP0->QEINT==DL_EQEP_INT_UNIT_TIME_OUT &&
        DL_GPIO_getEnabledInterrupts(GPIO0,HALL_MASK)==HALL_MASK &&
        NVIC_GetEnableIRQ(GPIO0_INT_IRQn) && NVIC_GetEnableIRQ(EQEP0_INT_IRQn);
}
void EQEP0_IRQHandler(void)
{
    const uint16_t flags=DL_EQEP_getInterruptStatus(EQEP0)&EQEP0->QEINT;
    if(!active || !flags)return; /* Old pending vector after disable. */
    DL_EQEP_clearInterruptStatus(EQEP0,flags|DL_EQEP_INT_GLOBAL);
    am13e_app_motor_on_hall_event(0,1);
}
void GPIO0_IRQHandler(void)
{
    const uint32_t flags=DL_GPIO_getEnabledInterruptStatus(GPIO0,UINT32_MAX);
    if(flags&~HALL_MASK)fail(); /* No silent loss of another GPIO client. */
    if(!active || !flags)return;
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    /* Freeze the hardware deadline before sampling status. Otherwise a
     * match between the timestamp read and flag clear could be lost. */
    DL_EQEP_disableUnitTimer(EQEP0);__DSB();
    const uint32_t ticks=EQEP0->QUTMR;
    const int timeout=(DL_EQEP_getInterruptStatus(EQEP0)&DL_EQEP_INT_UNIT_TIME_OUT)!=0U;
    DL_GPIO_clearInterruptStatus(GPIO0,flags);
    EQEP0->QUTMR=0U;
    DL_EQEP_clearInterruptStatus(EQEP0,DL_EQEP_INT_UNIT_TIME_OUT|DL_EQEP_INT_GLOBAL);
    NVIC_ClearPendingIRQ(EQEP0_INT_IRQn);
    DL_EQEP_enableUnitTimer(EQEP0,TIMEOUT_TICKS);
    /* Timeout wins over a coalesced edge, exactly as original TIM3 UIF. */
    am13e_app_motor_on_hall_event((int)(ticks/(AM13E_APP_MCLK_HZ/2000000U)),timeout);
    __set_PRIMASK(mask);
}
#else
enum { AM13E_HALL_NOT_SELECTED=0 };
#endif
