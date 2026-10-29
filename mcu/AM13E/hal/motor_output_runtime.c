#include "motor_bemf.h"
#include "../config.h"
#include "motor_output_runtime.h"
#if AM13E_MOTOR_OUTPUT_CONFIGURED
#include "motor_backend.h"
#include "motor_power_stage.h"
#include "board_motor_output_provider.h"
#include "motor_fault_route.h"
#ifdef SW_BLANKING
#include "motor_bemf.h"
#endif
#include <dl_mcpwm.h>
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE outputs[6]={
    DL_MCPWM_AQ_OUTPUT_1A,DL_MCPWM_AQ_OUTPUT_1B,DL_MCPWM_AQ_OUTPUT_2A,
    DL_MCPWM_AQ_OUTPUT_2B,DL_MCPWM_AQ_OUTPUT_3A,DL_MCPWM_AQ_OUTPUT_3B};
static const DL_MCPWM_COUNTER_COMPARE_MODULE compares[6]={
    DL_MCPWM_COUNTER_COMPARE_1A,DL_MCPWM_COUNTER_COMPARE_1B,DL_MCPWM_COUNTER_COMPARE_2A,
    DL_MCPWM_COUNTER_COMPARE_2B,DL_MCPWM_COUNTER_COMPARE_3A,DL_MCPWM_COUNTER_COMPARE_3B};
static volatile unsigned start_zero_events;
/* SIM_ONLY forensic marker: records why the EXISTING non-authorizing
 * motor_output_start() returned 0. This does not alter the OC, nFAULT,
 * comparator, MCPWM or gate acceptance criteria. Never use for release.
 */
#if defined(AM13E_E62_SIM_ONLY_MOTOR_FAULT_DIAG)
__attribute__((used)) volatile uint32_t e62_sim_motor_output_start_reject;
#define E62_OUTPUT_START_REJECT(n) (e62_sim_motor_output_start_reject=(n))
#else
#define E62_OUTPUT_START_REJECT(n) ((void)0)
#endif
static void output_fault(void)
{ am13e_app_motor_fault_shutdown();am13e_app_motor_fault_reset(); }
void am13e_motor_output_update_begin(void)
{
    /* PRIMASK does not stop hardware ZERO events. One-shot global load
     * excludes partial AQ/period/compare transfers during the transaction. */
    DL_MCPWM_clearGlobalLoadOneShotLatch(MOTOR_PWM);
    __DSB();
    if(DL_MCPWM_getGlobalLoadOneShotLatchStatus(MOTOR_PWM))output_fault();
    DL_MCPWM_enableGlobalLoadOneShotMode(MOTOR_PWM);
    DL_MCPWM_setGlobalLoadTrigger(MOTOR_PWM,DL_MCPWM_GL_LOAD_PULSE_CNTR_ZERO);
    DL_MCPWM_enableGlobalLoad(MOTOR_PWM);
}
void am13e_motor_output_stage(const uint16_t aq[6],const uint16_t compare[6],uint16_t period)
{
    if(__get_PRIMASK()!=1U)output_fault();
    /* Counts are in current TBCLK. Audio divider changes are handled by
     * the caller's DB configuration, not guessed electrical pulse widths. */
    const uint32_t red=MOTOR_PWM->DBRED & MCPWM_DBRED_DBRED_MASK;
    const uint32_t fed=(MOTOR_PWM->DBCTL & (UINT32_C(1)<<(DL_MCPWM_DB_FED+MCPWM_DBCTL_OUT_MODE_OFS))) ?
        MOTOR_PWM->DBFED & MCPWM_DBFED_DBFED_MASK : 0U;
#ifdef PWM_ENABLE
    if(red!=fed || period<=red)output_fault();
#else
    if(red<=fed || period<=red)output_fault();
#endif
    am13e_motor_output_update_begin();
    uint16_t image_aq[6],image_compare[6];
    DL_MCPWM_setTimeBasePeriodShadow(MOTOR_PWM,period);
    for(unsigned i=0;i<6U;++i){
        uint16_t action=aq[i];
#ifdef PWM_ENABLE
        /* Rel17 TIM1 PWM_ENABLE: positive PWM and sine enable the phase;
         * forced-low sink enables it; forced-high floating disables it.
         * NP inversion makes the complementary enable fall delayed, rise
         * immediate. MCPWM independent A RED / B FED realizes those edges.
         * Drag brake is PWM2 on enable, with the PWM input inactive.
         */
        uint32_t cc=compare[i];
        if(i&1U){
            const uint16_t pwm=DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA;
            if(aq[i-1U]==pwm || action==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO)
                action=DL_MCPWM_AQ_OUTPUT_LOW_ZERO;
            else if(action==0U)action=DL_MCPWM_AQ_OUTPUT_HIGH_ZERO;
            else if(cc==0U || cc>period)action=cc==0U ?
                DL_MCPWM_AQ_OUTPUT_HIGH_ZERO:DL_MCPWM_AQ_OUTPUT_LOW_ZERO;
        }else{
            if(action==0U || cc<=red)action=DL_MCPWM_AQ_OUTPUT_LOW_ZERO;
            else if(cc>period)action=DL_MCPWM_AQ_OUTPUT_HIGH_ZERO;
        }
#else
        uint32_t cc=compare[i]+((i&1U)?red:fed);
        if(action==0U)action=DL_MCPWM_AQ_OUTPUT_LOW_ZERO;
        else if((i&1U) && action==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO){
            /* Static sink: first ON after the previous high leg has turned
             * OFF, then retain ON across periods. Do not PWM every sink. */
            action=DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB;
            cc=red;
        }else if(!(i&1U) && compare[i]<=red-fed){
            action=DL_MCPWM_AQ_OUTPUT_LOW_ZERO;
        }else if(!(i&1U) && compare[i]>period){
            action=DL_MCPWM_AQ_OUTPUT_HIGH_ZERO; /* original FULL_DUTY */
        }else if((i&1U) && cc>period){
            action=DL_MCPWM_AQ_OUTPUT_LOW_ZERO; /* no wrapped freewheel edge */
        }
#endif
        /* Constant actions ignore compares. Keep these representable even
         * when a valid FULL_DUTY/near-limit image plus DT exceeds 16 bits. */
        if(action==DL_MCPWM_AQ_OUTPUT_LOW_ZERO || action==DL_MCPWM_AQ_OUTPUT_HIGH_ZERO)cc=1U;
        if(cc==0U)cc=1U; /* TRM26.6 zero-compare/shadow-load contention. */
        if(cc>UINT16_MAX)output_fault();
        image_aq[i]=action;image_compare[i]=(uint16_t)cc;
        DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM,compares[i],(uint16_t)cc);
        DL_MCPWM_setActionQualifierActionCompleteShadow(MOTOR_PWM,outputs[i],action);
    }
    if((MOTOR_PWM->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)==DL_MCPWM_COUNTER_MODE_STOP_FREEZE){
        if(!am13e_power_stage_inactive())output_fault();
        /* TRM26.6 immediate mode requires global load disabled. Seed the
         * entire first ACTIVE image with the external driver disabled;
         * no dependence on a ZERO/force pulse while TBCLK is stopped. */
        DL_MCPWM_disableGlobalLoad(MOTOR_PWM);
        DL_MCPWM_setTimeBasePeriodActive(MOTOR_PWM,period);
        for(unsigned i=0;i<6U;++i){
            DL_MCPWM_setCounterCompareActiveValue(MOTOR_PWM,compares[i],image_compare[i]);
            DL_MCPWM_setActionQualifierActionCompleteActive(MOTOR_PWM,outputs[i],image_aq[i]);
            if(DL_MCPWM_getCounterCompareActiveValue(MOTOR_PWM,compares[i])!=image_compare[i])output_fault();
        }
        if(DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM)!=period)output_fault();
        DL_MCPWM_enableGlobalLoad(MOTOR_PWM);
    }else DL_MCPWM_setGlobalLoadOneShotLatch(MOTOR_PWM);
}
int am13e_motor_output_start(void)
{
    if(__get_PRIMASK()!=1U) { E62_OUTPUT_START_REJECT(1U); return 0; }
    if(!am13e_app_motor_nfault_trip_ready())
        { E62_OUTPUT_START_REJECT(2U); return 0; }
    if(!am13e_app_motor_bemf_reference_start_ready())
        { E62_OUTPUT_START_REJECT(3U); return 0; }
    const AM13E_MotorPadRoute *route=am13e_board_motor_pad_route();
    am13e_power_stage_force_off();
    /* Clear retained AQ latches while the external driver is disabled.
     * Continuous LOW is released; ZERO/AQ/dead-band now own the signals. */
    for(unsigned i=0;i<6U;++i){
        DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM,outputs[i],DL_MCPWM_AQ_SW_ONE_SHOT_LOW);
        DL_MCPWM_forceActionQualifierSWAction(MOTOR_PWM,outputs[i]);
        DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM,outputs[i],DL_MCPWM_AQ_SW_FORCE_DISABLED);
    }
    if(!am13e_mcu_motor_pads_connect_pwm(route,AM13E_MOTOR_PAD_INVERT_MASK))
        { E62_OUTPUT_START_REJECT(4U); return 0; }
    if(!am13e_mcu_motor_pads_pwm_matches(route,AM13E_MOTOR_PAD_INVERT_MASK))
        { E62_OUTPUT_START_REJECT(5U); return 0; }
    if(!am13e_app_motor_nfault_trip_ready())
        { E62_OUTPUT_START_REJECT(6U); return 0; }
    /* Start the external driver only after one complete carrier cycle.
     * This drains retained DB state without busy-waiting under PRIMASK.
     * Core has already requested operation; this is peripheral startup,
     * not an additional application arming condition.
     */
    /* The ET generator retains a queued event even after INTFLAG clears.
     * Drain it while TBCLK is frozen, so an old ZERO cannot count toward
     * the new bridge's two-ZERO startup interval. */
    DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    DL_MCPWM_forceInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    __DSB();
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    if(DL_MCPWM_getEventTriggerEventCount(MOTOR_PWM,DL_MCPWM_ET_1)!=0U)
        { E62_OUTPUT_START_REJECT(7U); return 0; }
    start_zero_events=2U;
    DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_1,DL_MCPWM_EVT_TBCTR_ZERO);
    DL_MCPWM_setEventTriggerEventPrescale(MOTOR_PWM,DL_MCPWM_ET_1,1U);
    DL_MCPWM_enableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    return 1;
}
void am13e_motor_output_cancel(void)
{
    start_zero_events=0U;
    DL_MCPWM_clearGlobalLoadOneShotLatch(MOTOR_PWM);
    DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
}
int am13e_motor_output_start_pending(void) { return start_zero_events!=0U; }
int am13e_motor_output_zero_irq(void)
{
    if(!start_zero_events)return 0;
    if(!am13e_app_motor_nfault_trip_ready())output_fault();
    if(--start_zero_events==0U){
#ifdef AM13E_BOARD_SC5_CALIBRATION
        if(!am13e_app_motor_bemf_reference_gate_ready())output_fault();
#endif
#ifdef SW_BLANKING
        if(!am13e_app_motor_bemf_blanking_enabled())
#endif
        if(!am13e_app_motor_bemf_publication_pending())
            DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1);
        const uint32_t mask=__get_PRIMASK();
        __disable_irq();
        if(!am13e_app_motor_nfault_trip_ready() || !am13e_power_stage_enable() ||
           !am13e_app_motor_nfault_trip_ready())output_fault();
        __set_PRIMASK(mask);
    }
    return 1;
}
#else
enum { AM13E_MOTOR_OUTPUT_NEEDS_ELECTRICAL_BOARD_DEFINITIONS=0 };
#endif
