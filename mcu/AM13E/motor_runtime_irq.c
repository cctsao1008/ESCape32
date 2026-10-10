/* AM13E reference ESCape32 Rel17 AM13E FW1 interrupt release barrier.
 * Release PRIMASK for real PB14 DShot, 16 kHz SysTick, BEMF and motor
 * commutation IRQs; the Rel17 motor/audio runtime alone requests gate
 * drive later, not the IRQ release barrier.
 *
 * Require the installed TI startup vectors, enabled NVIC sources,
 * actual SysTick, initialized PB14 receiver, deasserted PB15 nFAULT,
 * and all six MCPWM pads forced LOW and isolated as GPIO inputs.
 *
 * IRQ release is not a power-stage arming operation. Default numeric
 * gate/dead-band model is not a claim of physical qualification.
 */
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_bemf.h"
#include "motor_event_timer.h"
#include "motor_fault_route.h"
#include "analog_runtime.h" /* ADC0 monitoring must have a live IRQ handler */
#include "irq_vectors.h"
#include "fault_input.h"
#include "command_capture.h"
#include "command_reply.h"
#include "clock_backend.h"
#include <soc.h>
#include <dl_mcpwm.h>
#include <dl_sysctl.h>
#include <stdint.h>

static volatile uint32_t irq_barrier_released;

static void irq_barrier_fault(void)
{
    __disable_irq();
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
    for (;;) { __NOP(); }
}

/* Cortex-M33 vectors must contain each real handler's Thumb address,
 * never the TI startup weak Default_Handler.
 */
static int vector_matches(IRQn_Type irq, void (*handler)(void))
{
    return NVIC_GetVector(irq) == ((uint32_t)(uintptr_t)handler | 1U);
}

void am13e_app_motor_runtime_enable_interrupts(void)
{
    if (__get_PRIMASK() != 1U || irq_barrier_released)
        irq_barrier_fault();

    /* The paired SW Architecture Baseline v1.6 requires exactly one
     * APP_BASE=0x6000 for both FW1/FW2. Boot must not transfer execution
     * to the legacy offset-vector (0x6800) image. All ISR/VTOR addresses
     * are verified against the actual installed vector table below.
     */
    const uint32_t vtor=SCB->VTOR;
    if (vtor != UINT32_C(0x00006000))
        irq_barrier_fault();

    if (!vector_matches(SysTick_IRQn,SysTick_Handler) ||
        !vector_matches(PendSV_IRQn,PendSV_Handler) ||
        !vector_matches(HardFault_IRQn,HardFault_Handler) ||
        !vector_matches(GPIO1_INT_IRQn,GPIO1_IRQHandler) ||
        !vector_matches(ADC0_INT1_INT_IRQn,ADC0_INT1_IRQHandler) ||
        !vector_matches(ECAP0_INT_IRQn,ECAP0_IRQHandler) ||
        !vector_matches(ECAP1_INT_IRQn,ECAP1_IRQHandler) ||
        !vector_matches(TIMG12_0_INT_IRQn,TIMG12_0_IRQHandler) ||
        !vector_matches(TIMG4_0_INT_IRQn,TIMG4_0_IRQHandler) ||
        !vector_matches(DMA0_INT_IRQn,DMA0_IRQHandler) ||
        !vector_matches(MCPWM0_INT_IRQn,MCPWM0_IRQHandler))
        irq_barrier_fault();

    if ((SysTick->CTRL & (SysTick_CTRL_ENABLE_Msk |
                          SysTick_CTRL_TICKINT_Msk)) !=
                         (SysTick_CTRL_ENABLE_Msk |
                          SysTick_CTRL_TICKINT_Msk) ||
        SysTick->LOAD != ((AM13E_APP_MCLK_HZ / AM13E_APP_SYSTICK_HZ)-1U) ||
        NVIC_GetPriority(SysTick_IRQn)!=0U ||
        NVIC_GetPriority(ECAP0_INT_IRQn)!=0U ||
        NVIC_GetPriority(ECAP1_INT_IRQn)!=0U ||
        NVIC_GetPriority(TIMG12_0_INT_IRQn)!=0U ||
        NVIC_GetPriority(TIMG4_0_INT_IRQn)!=0U ||
        NVIC_GetPriority(DMA0_INT_IRQn)!=0U ||
        NVIC_GetPriority(ADC0_INT1_INT_IRQn)!=1U)
        irq_barrier_fault();

    if (!NVIC_GetEnableIRQ(GPIO1_INT_IRQn) ||
        !NVIC_GetEnableIRQ(ECAP0_INT_IRQn) ||
        !NVIC_GetEnableIRQ(ECAP1_INT_IRQn) ||
        !NVIC_GetEnableIRQ(TIMG12_0_INT_IRQn) ||
        !NVIC_GetEnableIRQ(TIMG4_0_INT_IRQn) ||
        !NVIC_GetEnableIRQ(DMA0_INT_IRQn) ||
        !NVIC_GetEnableIRQ(ADC0_INT1_INT_IRQn))
        irq_barrier_fault();

    AM13E_PB14_Status rx={0};
    am13e_app_pb14_status(&rx);
    if (!rx.initialized || am13e_app_nfault_asserted() ||
        !am13e_app_motor_nfault_trip_ready() ||
        !am13e_app_motor_inactive_preflight_ok() ||
        (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK) != 0U)
        irq_barrier_fault();

    /* Runtime IRQs can now execute. PB13 is inactive and physical PWM
     * pins are isolated until Rel17 issues an actual drive/audio request.
     */
    __DSB();
    __ISB();
    irq_barrier_released=1U;
    __enable_irq();
}
