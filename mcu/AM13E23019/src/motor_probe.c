/*
 * AM13E23019 MCPWM backend compile probe.
 *
 * This image is not a motor-running firmware.  It only type-checks and links
 * the first ESCape32 motor-backend operations against the pinned TI SDK.
 * TBCLK is never enabled, so this probe does not intentionally start PWM.
 */

#include "ti_sdk_dl_config.h"
#include "dl_ecap.h"
#include "dl_timerg.h"
#include "hw_bemf_am13e_events.h"
#include "dl_cmpss_lite.h"

#define ESCAPE32_AM13E_MCPWM_INST MCPWM_1_INST
#include "hw_motor_am13e.h"


/*
 * FW1 hardware route compile probe: AM13E eCAP supports direct CMPSS
 * CTRIPH/CTRIPL event input selection. No eCAP or comparator is enabled.
 * The E62 board CMPSS pin routing and BEMF edge polarity are NOT inferred.
 */
static DL_ECAP_INPUT fw1_bemf_ecap_input(unsigned comparator, bool high)
{
    switch (comparator) {
    case 0: return high ? DL_ECAP_INPUT_CMPSS0_CTRIPH : DL_ECAP_INPUT_CMPSS0_CTRIPL;
    case 1: return high ? DL_ECAP_INPUT_CMPSS1_CTRIPH : DL_ECAP_INPUT_CMPSS1_CTRIPL;
    case 3: return high ? DL_ECAP_INPUT_CMPSS3_CTRIPH : DL_ECAP_INPUT_CMPSS3_CTRIPL;
    default: return DL_ECAP_INPUT_CMPSS0_CTRIPH; /* probe only; no runtime selection */
    }
}

static void fw1_bemf_ecap_compile_probe(void)
{
    /* Compile-time SDK API verification only, held behind run_probe == 0. */
    DL_ECAP_selectECAPInput(ECAP0, fw1_bemf_ecap_input(0, true));
    DL_ECAP_selectECAPInput(ECAP0, fw1_bemf_ecap_input(1, false));
    DL_ECAP_selectECAPInput(ECAP0, fw1_bemf_ecap_input(3, true));
    DL_ECAP_setEventPolarity(ECAP0, DL_ECAP_EVENT_1, DL_ECAP_EVENT_RISING_EDGE);
    DL_ECAP_enableTimeStampCapture(ECAP0);
    (void)DL_ECAP_getEventTimeStamp(ECAP0, DL_ECAP_EVENT_1);
    DL_ECAP_disableTimeStampCapture(ECAP0);
}

/*
 * FW1 event-chain API integration. No NVIC, TBCLK, power stage or actual
 * interrupts are enabled in this compile-only image.
 * TIMG12 must be configured as a 32-bit down-counter with ZERO interrupt
 * in a later product-specific initialization step.
 */
static am13e_bemf_event_engine_t fw1_event_engine;
static void fw1_arm_delay(void *unused, uint32_t ticks)
{
    (void)unused;
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_setLoadValue(TIMG12, ticks);
    DL_TimerG_setTimerCount(TIMG12, ticks);
    DL_TimerG_startCounter(TIMG12);
}
static void fw1_cancel_delay(void *unused)
{
    (void)unused;
    DL_TimerG_stopCounter(TIMG12);
    DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}
static void fw1_commutation_due(void *unused)
{
    (void)unused;
    /* Gate-output commutation stays HW-Pending. No MCPWM action here. */
}
static void fw1_capture_event_compile_probe(void)
{
    am13e_bemf_state_t initial = { .interval = 1000U,
        .electrical_time = 100000000U, .sync = 0U, .fast = false };
    const am13e_bemf_event_ops_t ops = {
        fw1_arm_delay, fw1_cancel_delay, fw1_commutation_due, 0 };
    if (!am13e_bemf_event_init(&fw1_event_engine, &ops, initial, 0U))
        return;
    if (DL_ECAP_getInterruptSource(ECAP0) & DL_ECAP_ISR_SOURCE_CEVT1) {
        uint32_t timestamp = DL_ECAP_getEventTimeStamp(ECAP0, DL_ECAP_EVENT_1);
        DL_ECAP_clearInterrupt(ECAP0, DL_ECAP_ISR_SOURCE_CEVT1);
        (void)am13e_bemf_event_capture(&fw1_event_engine, timestamp);
    }
    if (DL_TimerG_getPendingInterrupt(TIMG12) == DL_TIMERG_IIDX_ZERO) {
        DL_TimerG_clearInterruptStatus(TIMG12, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        am13e_bemf_event_delay_elapsed(&fw1_event_engine);
    }
}

static void am13e_motor_backend_compile_probe(void)
{
    /* Exercise executable FW1 start/commutate/stop code in the compile
     * probe; run_probe remains false in the bench fixture's main().
     */
    (void)hw_motor_am13e_runtime_start(8000U, 40U, 2000U);
    (void)hw_motor_am13e_runtime_commutate(4U, 2U, true);
    (void)hw_motor_am13e_runtime_commutate(0U, 0U, false);
    hw_motor_am13e_runtime_stop();
    hw_motor_am13e_configure_split_deadband(40);
    hw_motor_sine_update(8000, 1000, 2000, 4000);

    /*
     * Stage D: compile-check the real ESCape32 phase-state encoder.
     *
     * Step 1: +C / -B / A floating.  Exercise both damp modes, then exercise
     * the zero-throttle coast case where all three phases float.
     */
    hw_motor_am13e_encode_six_step_shadow(0x4U, 0x2U, true);
    hw_motor_am13e_encode_six_step_shadow(0x4U, 0x2U, false);
    hw_motor_am13e_encode_six_step_shadow(0x0U, 0x0U, false);

    /*
     * Stage F: compile-check the selected commutation candidate.
     *
     * The AQ event tables are configured once as PWM carriers. Six-step
     * commutation uses only asynchronous pair-local continuous software force.
     * This keeps duty/period shadows in the PWM-boundary timing domain.
     */
    hw_motor_am13e_prepare_pwm_carriers_active();
    hw_motor_am13e_apply_six_step_force_probe(0x4U, 0x2U, true);
    hw_motor_am13e_apply_six_step_force_probe(0x4U, 0x2U, false);
    hw_motor_am13e_apply_six_step_force_probe(0x0U, 0x0U, false);

    /*
     * Stage C: verify the exact AQ shadow API used by the TI global-load
     * example for all three phase pairs.  These values are compile probes
     * only; they are not yet the ESCape32 six-step gate-state mapping.
     */
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_1A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_1B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_2A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_2B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_3A,
        DL_MCPWM_AQ_OUTPUT_LOW,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DL_MCPWM_setActionQualifierActionShadow(
        MCPWM_1_INST,
        DL_MCPWM_AQ_OUTPUT_3B,
        DL_MCPWM_AQ_OUTPUT_HIGH,
        DL_MCPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    /*
     * The SDK exposes pair-local continuous software force. Stage F now uses
     * this as the commutation selector, with an all-FLOAT mask between states.
     * Keep these direct calls as an API-level compile check as well.
     */
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_SW_FORCE_DISABLED);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_2B, DL_MCPWM_AQ_SW_FORCE_DISABLED);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_3A, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(
        MCPWM_1_INST, DL_MCPWM_AQ_OUTPUT_3B, DL_MCPWM_AQ_SW_CONTINUOUS_LOW);

    hw_motor_commit_update();
}

int main(void)
{
    /*
     * Keep the probe function compiled and type-checked without executing it.
     * The volatile condition prevents the compiler front-end from discarding
     * the function body before semantic/API checking.
     */
    volatile int run_probe = 0;

    if (run_probe) {
        am13e_motor_backend_compile_probe();
        fw1_bemf_ecap_compile_probe();
        fw1_capture_event_compile_probe();
    }

    for (;;) {
    }
}
