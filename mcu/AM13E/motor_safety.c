/* ESCape32 Rel17 on AM13E23019: first physical MCPWM0 foundation.
 *
 * PURPOSE: configure an INACTIVE MCPWM0 and keep all six product pads
 * as digital high-impedance inputs. A hardware comparator->PWMXBAR->MCPWM
 * overcurrent trip, deadtime, PB13 driver-enable polarity and power-stage
 * output waveforms have NOT been qualified; this code NEVER arms the bridge.
 *
 * This is genuine register/DriverLib work, not a link-only motor stub.
 * The unresolved am13e_app_motor_runtime_enable_interrupts() remains the
 * product arming barrier; do not provide a no-op to make FW1 link.
 */
#include "motor_backend.h"
#include "motor_event_timer.h"
#include "motor_safety.h"
#include "motor_shadow_plan.h"
#include <soc.h>
#include <dl_mcpwm.h>
#include <dl_gpio.h>
#include <stddef.h>
#include <stdint.h>

/* Baseline v1.6: U=PA8/PA11, V=PA9/PA30, W=PA10/PA31. */
#define PWM_PADS (DL_GPIO_PIN(8U) | DL_GPIO_PIN(11U) | \
                  DL_GPIO_PIN(9U) | DL_GPIO_PIN(30U) | \
                  DL_GPIO_PIN(10U) | DL_GPIO_PIN(31U))

_Static_assert(IOMUX_PINCM_PA8 == 8 && IOMUX_PINCM_PA11 == 11 &&
               IOMUX_PINCM_PA9 == 9 && IOMUX_PINCM_PA30 == 30 &&
               IOMUX_PINCM_PA10 == 10 && IOMUX_PINCM_PA31 == 31,
               "AM13E MCPWM0 pin assignment changed");
_Static_assert(IOMUX_PA8_MCPWM0_1A == 7U && IOMUX_PA11_MCPWM0_1B == 7U &&
               IOMUX_PA9_MCPWM0_2A == 7U && IOMUX_PA30_MCPWM0_2B == 7U &&
               IOMUX_PA10_MCPWM0_3A == 7U && IOMUX_PA31_MCPWM0_3B == 5U,
               "AM13E MCPWM0 alternate function map changed");
_Static_assert(DL_MCPWM_COUNTER_MODE_STOP_FREEZE == 2U,
               "MCPWM Stop/Freeze register encoding changed");

static volatile uint32_t safety_initialized;
static volatile uint32_t fault_latched;
static volatile uint32_t last_trip_irq_flags;
static volatile uint32_t last_trip_zone_flags;

/* MCU pad ownership: never change PB13 power enable or PB15 nFAULT.
 * Hi-Z is only an MCU-side staging state; gate-driver input bias and
 * actual inverter shutdown must still be verified on the final board.
 */
static void disconnect_pwm_pads(void)
{
    DL_GPIO_enablePower(GPIO0);
    if (!DL_GPIO_isPowerEnabled(GPIO0)) {
        for (;;) { __NOP(); }
    }
    DL_GPIO_disableOutput(GPIO0, PWM_PADS);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA8);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA11);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA9);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA30);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA10);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PA31);
}

static int pwm_pads_disconnected(void)
{
    return (GPIO0->DOE31_0 & PWM_PADS) == 0U &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA8) &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA11) &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA9) &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA30) &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA10) &&
           DL_GPIO_isInputEnabled(IOMUX_PINCM_PA31) &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA8) == IOMUX_PA8_GPIO08 &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA11) == IOMUX_PA11_GPIO11 &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA9) == IOMUX_PA9_GPIO09 &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA30) == IOMUX_PA30_GPIO30 &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA10) == IOMUX_PA10_GPIO10 &&
           DL_GPIO_getPeripheralFunctionBits(IOMUX_PINCM_PA31) == IOMUX_PA31_GPIO31;
}

static void force_pwm_inactive(void)
{
    /* The timebase is stopped and each AQ output continuously low.
     * This is complementary to pad disconnect, not a proven physical
     * gate-driver disable or a substitute for hardware Trip Zone.
     */
    DL_MCPWM_disableTBCLK();
    DL_MCPWM_setTimeBaseCounterMode(MCPWM0, DL_MCPWM_COUNTER_MODE_STOP_FREEZE);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_1A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_1B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_2A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_2B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_3A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MCPWM0, DL_MCPWM_AQ_OUTPUT_3B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
}

static int pwm_registers_inactive(void)
{
    const uint32_t expected = (uint32_t)DL_MCPWM_AQ_SW_CONTINUOUS_LOW *
                                  (UINT32_C(1) + (UINT32_C(1) << 4U));
    return (MCPWM0->TBCTL & MCPWM_TBCTL_CTRMODE_MASK) ==
               (uint32_t)DL_MCPWM_COUNTER_MODE_STOP_FREEZE &&
           (MCPWM0->PWM1_AQSFRC & (MCPWM_PWM1_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM1_AQSFRC_PWMB_MASK)) == expected &&
           (MCPWM0->PWM2_AQSFRC & (MCPWM_PWM2_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM2_AQSFRC_PWMB_MASK)) == expected &&
           (MCPWM0->PWM3_AQSFRC & (MCPWM_PWM3_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM3_AQSFRC_PWMB_MASK)) == expected;
}

void am13e_app_motor_init(void)
{
    /* Rel17 main() calls here after real clock/initgpio/input setup.
     * Boot must still hold PRIMASK while motor power is not qualified.
     */
    if (__get_PRIMASK() == 0U) {
        __disable_irq();
        fault_latched = 1U;
        for (;;) { __NOP(); }
    }
    safety_initialized = 0U;
    fault_latched = 0U;
    disconnect_pwm_pads();
    DL_MCPWM_disableTBCLK();

    DL_MCPWM_Config config;
    DL_MCPWM_initParamsSetDefault(&config);
    config.timeBaseConfig.counterMode = DL_MCPWM_COUNTER_MODE_STOP_FREEZE;
    config.actionQualifierConfig.aqPwm1A.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    config.actionQualifierConfig.aqPwm1B.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    config.actionQualifierConfig.aqPwm2A.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    config.actionQualifierConfig.aqPwm2B.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    config.actionQualifierConfig.aqPwm3A.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    config.actionQualifierConfig.aqPwm3B.pwmSwForceAction =
        DL_MCPWM_AQ_SW_CONTINUOUS_LOW;
    /* No hardware TZ source is claimed until OC comparator, PWMXBAR
     * and physically safe action/polarities have been checked.
     */
    config.tripZoneConfig.enableTripZoneMask = 0U;
    config.tripZoneConfig.actionOnA = DL_MCPWM_TZ_ACTION_HIGH_Z;
    config.tripZoneConfig.actionOnB = DL_MCPWM_TZ_ACTION_HIGH_Z;
    DL_MCPWM_init(MCPWM0, &config);
    force_pwm_inactive();
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    am13e_app_motor_timing_init();
    safety_initialized = 1U; /* Inactive-preflight only, NOT motor ready. */
}

/* E1-AR real MCPWM0 period and six compare SHADOW writes. MCPWM TBCLK
 * stays STOP/FREEZE; AQ forced low; all six pins GPIO input/Hi-Z.
 * No dead-time, trip routing, drive polarity or output enable is implied.
 * Software staging is only one prerequisite for an eventual motor port.
 */
int am13e_app_motor_stage_inactive_shadow(const AM13E_MotorShadowPlan *plan)
{
    if (!am13e_motor_shadow_validate(plan)) return 0;
    const uint32_t previous_primask=__get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched) {
        __set_PRIMASK(previous_primask);
        return 0;
    }
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }

    static const DL_MCPWM_COUNTER_COMPARE_MODULE channels[6] = {
        DL_MCPWM_COUNTER_COMPARE_1A, DL_MCPWM_COUNTER_COMPARE_1B,
        DL_MCPWM_COUNTER_COMPARE_2A, DL_MCPWM_COUNTER_COMPARE_2B,
        DL_MCPWM_COUNTER_COMPARE_3A, DL_MCPWM_COUNTER_COMPARE_3B
    };
    DL_MCPWM_setTimeBasePeriodShadow(MCPWM0, plan->period);
    for (unsigned i=0U;i<6U;++i)
        DL_MCPWM_setCounterCompareShadowValue(MCPWM0,channels[i],plan->compare[i]);

    if (DL_MCPWM_getTimeBasePeriodShadow(MCPWM0)!=plan->period) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    for (unsigned i=0U;i<6U;++i) {
        if (DL_MCPWM_getCounterCompareShadowValue(MCPWM0,channels[i])!=plan->compare[i]) {
            am13e_app_motor_fault_shutdown();
            am13e_app_motor_fault_reset();
        }
    }
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    __set_PRIMASK(previous_primask);
    return 1;
}

/* Rel17 util.c::resetcom(): restore *physical MCU-side inactive bridge*
 * around score/PCM playback. Never clear hardware Trip Zone or fault
 * latch, never assume PB13 gate enable polarity, and never substitute
 * a Software AQ command for the independent overcurrent hardware trip.
 *
 * Real output writes plus read-back, not a dummy link-only callback.
 * Other audio functions remain missing until their actual hardware
 * timing/output ownership is implemented.
 */
void am13e_app_commutation_reset(void)
{
    const uint32_t irqmask = __get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    force_pwm_inactive();
    disconnect_pwm_pads();
    if (!pwm_registers_inactive() || !pwm_pads_disconnected()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    __set_PRIMASK(irqmask);
}

void am13e_app_motor_fault_shutdown(void)
{
    /* This is a real software shutdown fallback; MCPWM HW Trip Zone
     * must be implemented separately for bounded-latency overcurrent.
     * Unknown gate-enable polarity means this does not establish
     * hardware gate-off proof and cannot authorize motor operation.
     */
    __disable_irq();
    fault_latched = 1U;
    force_pwm_inactive();
    disconnect_pwm_pads();
}

/* Strong AM13E230x MCPWM0 startup vector. Only the actual hardware
 * Trip/overcurrent routing can enforce bounded-latency shutdown without
 * waiting for this CPU interrupt. Unexpected MCPWM interrupts also fault.
 */
void MCPWM0_IRQHandler(void)
{
    last_trip_irq_flags = DL_MCPWM_getInterruptSource(MCPWM0);
    last_trip_zone_flags = DL_MCPWM_getTripZoneFlagStatus(MCPWM0);
    DL_MCPWM_clearInterrupt(MCPWM0, (uint16_t)last_trip_irq_flags);
    DL_MCPWM_clearGlobalInterrupt(MCPWM0);
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
}

void am13e_app_motor_fault_reset(void)
{
    /* No auto-reset until board-specific gate-off polarity and hardware
     * trip routing are implemented. An unconditional reset could re-arm
     * an already unsafe power stage. Leave an inspectable latched fault.
     * Rel17 hard_fault_handler() expects this function not to return.
     */
    __disable_irq();
    fault_latched = 1U;
    for (;;) { __NOP(); }
}

void am13e_app_motor_trip_snapshot(uint32_t *irq, uint32_t *tz)
{
    if (irq != NULL) *irq = last_trip_irq_flags;
    if (tz != NULL) *tz = last_trip_zone_flags;
}

/* Diagnostics accessible through a debugger; never read as proof that
 * the external half-bridge or hardware overcurrent trip is qualified.
 */
uint32_t am13e_app_motor_inactive_preflight_ok(void)
{
    return safety_initialized && !fault_latched &&
           pwm_registers_inactive() && pwm_pads_disconnected();
}
