#include "../config.h"
/* Original Rel17 motor policy -> native MOTOR_PWM AQ/compare/DB and pins.
 * A complete electrical board definition selects physical output operation.
 * Without those inputs the Reference remains isolated and unqualified. */
#include "motor_backend.h"
#include "motor_audio_hw.h" /* Exclusive MCPWM Motor/Audio resource owner */
#include "motor_output_backend.h"
#include "board_motor_output_provider.h"
#include "board_configuration.h" /* Required physical pin selection */
#include "motor_event_timer.h"
#include "motor_bemf.h"
#include "motor_power_stage.h" /* PB13 GPIO inactive-only initializer */
#include "motor_ownership_plan.h" /* Active MPCWM0 Motor/Music/PCM owner */
#include "motor_safety.h"
#include "motor_output_runtime.h"
#include "motor_fault_route.h"
#include "input_watchdog.h"
#include "motor_shadow_plan.h"
#include "motor_aq_plan.h"
#include "motor_pwm_shadow_plan.h"
#include "util_backend.h" /* real Rel17 resetcom() prototype */
#include "clock_backend.h"
#include "fault_input.h" /* PB15 nFAULT input supervision */
#include <soc.h>
#include <dl_mcpwm.h>
#include <dl_sysctl.h>
#include <dl_gpio.h>
#include <stddef.h>
#include <stdint.h>

/* Unmodified Rel17 360-sample waveform, linked from motor_sine_table.c. */
extern const uint16_t sinedata[];

_Static_assert(DL_MCPWM_COUNTER_MODE_STOP_FREEZE == 2U,
               "MCPWM Stop/Freeze register encoding changed");

static volatile uint32_t safety_initialized;
static volatile uint32_t fault_latched;
static volatile uint32_t motor_timebase_running; /* Native MOTOR_PWM counter state */
/* 0=Drive/idle, 1=Rel17 Music, 2=Rel17 AU/PCM. Configured audio uses the same physical output adapter. */
static volatile uint32_t audio_owner;
/* Prevent re-entering a failed pad disconnect from the existing reset
 * path. Gate disable and PWM freeze precede that disconnect attempt. */
static volatile uint32_t fault_shutdown_in_progress;
static volatile AM13E_MotorOwner motor_owner_mode;
static volatile uint32_t audio_period_valid;

uint32_t am13e_app_motor_audio_mode(void)
{
    return audio_owner;
}


#if AM13E_MOTOR_OUTPUT_CONFIGURED
/* Register transaction cache only: all phase/duty decisions stay in Core.
 * Shadow writes inhibit global loads until the complete electrical image
 * is committed. PRIMASK alone cannot exclude a peripheral ZERO event.
 */
_Static_assert(DL_MCPWM_AQ_OUTPUT_1A==0 && DL_MCPWM_AQ_OUTPUT_3B==5 &&
    DL_MCPWM_COUNTER_COMPARE_1A==0 && DL_MCPWM_COUNTER_COMPARE_3B==5,
    "Six-output transaction cache requires the native SDK channel encodings");
static uint16_t logical_aq[6],logical_compare[6];
static unsigned physical_shadow_dirty,physical_runtime_image;
/* Exclude inactive diagnostic proofs from runtime electrical conversion.
 * This is register transaction bookkeeping, not motor authorization. */
static void output_aq_shadow(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o,uint16_t a)
{
    if(!physical_runtime_image){DL_MCPWM_setActionQualifierActionCompleteShadow(p,o,a);return;}
    if(!physical_shadow_dirty)am13e_motor_output_update_begin();
    /* Runtime logical images are converted and written ONCE by stage().
     * The old logical hardware write was overwritten before global load. */
    logical_aq[(unsigned)o]=a;physical_shadow_dirty=1U;
}
static void output_compare_shadow(MCPWM_Regs *p,DL_MCPWM_COUNTER_COMPARE_MODULE c,uint16_t value)
{
    if(!physical_runtime_image){DL_MCPWM_setCounterCompareShadowValue(p,c,value);return;}
    if(!physical_shadow_dirty)am13e_motor_output_update_begin();
    logical_compare[(unsigned)c]=value;physical_shadow_dirty=1U;
}
static void output_period_shadow(MCPWM_Regs *p,uint16_t value)
{
    if(!physical_runtime_image){DL_MCPWM_setTimeBasePeriodShadow(p,value);return;}
    if(!physical_shadow_dirty)am13e_motor_output_update_begin();
    physical_shadow_dirty=1U;
    DL_MCPWM_setTimeBasePeriodShadow(p,value);
}
static void output_commit_shadow(void)
{
    if(physical_shadow_dirty && safety_initialized && !fault_latched){
        physical_shadow_dirty=0U;
        am13e_motor_output_stage(logical_aq,logical_compare,DL_MCPWM_getTimeBasePeriodShadow(MOTOR_PWM));
    }
}
static void output_active_begin(void)
{
    if((MOTOR_PWM->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)!=DL_MCPWM_COUNTER_MODE_STOP_FREEZE ||
       !am13e_power_stage_inactive()){
        am13e_app_motor_fault_shutdown();am13e_app_motor_fault_reset();
    }
    DL_MCPWM_disableGlobalLoad(MOTOR_PWM);
    if(physical_runtime_image)physical_shadow_dirty=1U;
}
static void output_aq_active(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o,uint16_t a)
{ output_active_begin();DL_MCPWM_setActionQualifierActionCompleteActive(p,o,a); }
static void output_compare_active(MCPWM_Regs *p,DL_MCPWM_COUNTER_COMPARE_MODULE c,uint16_t a)
{ output_active_begin();DL_MCPWM_setCounterCompareActiveValue(p,c,a); }
static void output_period_active(MCPWM_Regs *p,uint16_t a)
{ output_active_begin();DL_MCPWM_setTimeBasePeriodActive(p,a); }
static void motor_transaction_end(uint32_t mask)
{
    output_commit_shadow();
    __set_PRIMASK(mask);
}
#else
/* Unconfigured reference still executes the actual SDK writes and original
 * inactive proofs. These wrappers add no physical-output authorization. */
static inline void output_aq_shadow(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o,uint16_t a)
{ DL_MCPWM_setActionQualifierActionCompleteShadow(p,o,a); }
static inline void output_compare_shadow(MCPWM_Regs *p,DL_MCPWM_COUNTER_COMPARE_MODULE c,uint16_t a)
{ DL_MCPWM_setCounterCompareShadowValue(p,c,a); }
static inline void output_period_shadow(MCPWM_Regs *p,uint16_t a)
{ DL_MCPWM_setTimeBasePeriodShadow(p,a); }
static inline void output_aq_active(MCPWM_Regs *p,DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE o,uint16_t a)
{ DL_MCPWM_setActionQualifierActionCompleteActive(p,o,a); }
static inline void output_compare_active(MCPWM_Regs *p,DL_MCPWM_COUNTER_COMPARE_MODULE c,uint16_t a)
{ DL_MCPWM_setCounterCompareActiveValue(p,c,a); }
static inline void output_period_active(MCPWM_Regs *p,uint16_t a)
{ DL_MCPWM_setTimeBasePeriodActive(p,a); }
static inline void motor_transaction_end(uint32_t mask) { __set_PRIMASK(mask); }

#endif

static volatile uint32_t motor_timebase_starts;
static volatile uint32_t motor_timebase_stops;
static volatile uint32_t sine_entry_pending;
static volatile uint32_t sine_mode_active;
static volatile uint32_t drag_brake_aq_staged; /* original drag-brake AQ staged */
static volatile uint32_t lock_brake_aq_staged; /* stopped Rel17 hold image */
static volatile uint32_t sine_write_count;
static volatile uint32_t last_trip_irq_flags;
static volatile uint32_t last_trip_zone_flags;

/* Board provider determines six pins; this logic remains a shared
 * fail-closed MOTOR MOTOR_PWM owner (including music and AU/PCM audio).
 * Disconnect must remain callable from the fault path even with IRQs.
 */
static void disconnect_pwm_pads(void)
{
    if (!am13e_mcu_motor_pads_disconnect(am13e_board_motor_pad_route()))
        am13e_app_motor_fault_reset();
}

static int pwm_pads_disconnected(void)
{
    return am13e_mcu_motor_pads_disconnected(
        am13e_board_motor_pad_route());
}

static void force_pwm_inactive(void)
{
    /* PB13 inactive FIRST (when board-configured), then six-pad
     * disconnect, and only then freeze the internal PWM timebase.
     */
    am13e_power_stage_force_off();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    am13e_motor_output_cancel();
#endif
    /* The timebase is stopped and each AQ output continuously low.
     * This is complementary to pad disconnect, not a proven physical
     * gate-driver disable or a substitute for hardware Trip Zone.
     */
    DL_MCPWM_disableTBCLK();
    DL_MCPWM_setTimeBaseCounterMode(MOTOR_PWM, DL_MCPWM_COUNTER_MODE_STOP_FREEZE);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_1A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_1B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_2A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_2B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_3A,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
    DL_MCPWM_setActionQualifierSWAction(MOTOR_PWM, DL_MCPWM_AQ_OUTPUT_3B,
                                        DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
}

static int pwm_io_for_runtime(void)
{
    /* Supports live Motor/Audio AQ and Compare updates on a qualified
     * bridge. Default IO Plan still accepts GPIO INPUT/Hi-Z only.
     */
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    return pwm_pads_disconnected() ||
        (am13e_app_motor_nfault_trip_ready() &&
         am13e_mcu_motor_pads_pwm_matches(am13e_board_motor_pad_route(),AM13E_MOTOR_PAD_INVERT_MASK));
#else
    return pwm_pads_disconnected();
#endif
}

static int pwm_registers_inactive(void)
{
    const uint32_t expected = (uint32_t)DL_MCPWM_AQ_SW_CONTINUOUS_LOW *
                                  (UINT32_C(1) + (UINT32_C(1) << 4U));
    return (MOTOR_PWM->TBCTL & MCPWM_TBCTL_CTRMODE_MASK) ==
               (uint32_t)DL_MCPWM_COUNTER_MODE_STOP_FREEZE &&
           (MOTOR_PWM->PWM1_AQSFRC & (MCPWM_PWM1_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM1_AQSFRC_PWMB_MASK)) == expected &&
           (MOTOR_PWM->PWM2_AQSFRC & (MCPWM_PWM2_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM2_AQSFRC_PWMB_MASK)) == expected &&
           (MOTOR_PWM->PWM3_AQSFRC & (MCPWM_PWM3_AQSFRC_PWMA_MASK |
                                    MCPWM_PWM3_AQSFRC_PWMB_MASK)) == expected;
}


/* Real MCPWM DBCTL/DBRED/DBFED: G431-derived numeric default is
 * configured for runtime, NOT proof of electrically safe non-overlap.
 */
#ifdef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
#if AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED != 1
#error "AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED must be 1"
#endif
#if !defined(AM13E_MOTOR_DB_RED_TICKS) || \
    !defined(AM13E_MOTOR_DB_FED_TICKS) || \
    !defined(AM13E_MOTOR_DB_RED_POLARITY) || \
    !defined(AM13E_MOTOR_DB_FED_POLARITY) || \
    !defined(AM13E_MOTOR_DB_RED_INPUT) || \
    !defined(AM13E_MOTOR_DB_FED_INPUT) || \
    !defined(AM13E_MOTOR_DB_SWAP_A) || \
    !defined(AM13E_MOTOR_DB_SWAP_B) || \
    !defined(AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS)
#error "AM13E reference verified dead-band requires all physical polarity/input/delay fields"
#endif
_Static_assert(AM13E_MOTOR_DB_RED_TICKS > 0 &&
               AM13E_MOTOR_DB_RED_TICKS < 0x4000 &&
               AM13E_MOTOR_DB_FED_TICKS >= 0 &&
               AM13E_MOTOR_DB_FED_TICKS < 0x4000 &&
               AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS >= 0 &&
               AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS < 0x4000 &&
               (AM13E_MOTOR_DB_RED_POLARITY == 0 ||
                AM13E_MOTOR_DB_RED_POLARITY == 1) &&
               (AM13E_MOTOR_DB_FED_POLARITY == 0 ||
                AM13E_MOTOR_DB_FED_POLARITY == 1) &&
               (AM13E_MOTOR_DB_RED_INPUT == DL_MCPWM_DB_INPUT_PWMA ||
                AM13E_MOTOR_DB_RED_INPUT == DL_MCPWM_DB_INPUT_PWMB) &&
               (AM13E_MOTOR_DB_FED_INPUT == DL_MCPWM_DB_INPUT_PWMA ||
                AM13E_MOTOR_DB_FED_INPUT == DL_MCPWM_DB_INPUT_PWMB ||
                AM13E_MOTOR_DB_FED_INPUT == DL_MCPWM_DB_INPUT_DB_RED) &&
               (AM13E_MOTOR_DB_SWAP_A == 0 || AM13E_MOTOR_DB_SWAP_A == 1) &&
               (AM13E_MOTOR_DB_SWAP_B == 0 || AM13E_MOTOR_DB_SWAP_B == 1),
               "Invalid board-verified MCPWM dead-band topology/delay");
#endif

static void configure_motor_deadband_isolated(void)
{
    /* Called after actual MCPWM init while TBCLK is frozen, all AQ SW
     * actions are forced LOW and all six physical pads are GPIO inputs.
     * Configuring these device registers does not energize a half-bridge.
     */
#ifdef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
    DL_MCPWM_DeadBandConfig db = {
        .enableRisingEdgeDelayOnPathA=true,
        .enableFallingEdgeDelayOnPathB=(AM13E_MOTOR_DB_FED_TICKS!=0),
        .enableOutputSwapA=(AM13E_MOTOR_DB_SWAP_A!=0),
        .enableOutputSwapB=(AM13E_MOTOR_DB_SWAP_B!=0),
        .risingEdgeDelayPolarity=
             (DL_MCPWM_DEADBAND_POLARITY)AM13E_MOTOR_DB_RED_POLARITY,
        .fallingEdgeDelayPolarity=
             (DL_MCPWM_DEADBAND_POLARITY)AM13E_MOTOR_DB_FED_POLARITY,
        .risingEdgeDelayInputSource=AM13E_MOTOR_DB_RED_INPUT,
        .fallingEdgeDelayInputSource=AM13E_MOTOR_DB_FED_INPUT,
        .risingEdgeDelayCount=AM13E_MOTOR_DB_RED_TICKS,
        .fallingEdgeDelayCount=AM13E_MOTOR_DB_FED_TICKS?AM13E_MOTOR_DB_FED_TICKS:1U
    };
    /* TRM26.7.2: global ZERO loading forbids a zero DB count, even
     * though global loading overrides the individual FREEZE modes.
     * FED=0 means bypass B's delay path; retain a harmless nonzero count
     * in its unused ACTIVE/SHADOW registers. B compare still supplies DT.
     */
    DL_MCPWM_configureDeadBand(MOTOR_PWM,&db);
    /* Explicit fixed-delay shadow handling: do not transfer an
     * unqualified value at an arbitrary PWM Zero/Period edge.
     */
    DL_MCPWM_setRisingEdgeDelayCountShadowLoadMode(
        MOTOR_PWM,DL_MCPWM_RED_LOAD_FREEZE);
    DL_MCPWM_setFallingEdgeDelayCountShadowLoadMode(
        MOTOR_PWM,DL_MCPWM_FED_LOAD_FREEZE);
    /* Validate ALL configured DBCTL topology bits, not only delay count.
     * A wrong input selection, output swap, or polarity may otherwise
     * defeat non-overlap even if DBRED/DBFED read back correctly.
     */
    const uint32_t dbctl=MOTOR_PWM->DBCTL;
    const uint32_t fed_input=AM13E_MOTOR_DB_FED_INPUT;
    uint32_t mask=MCPWM_DBCTL_OUT_MODE_MASK |
                  MCPWM_DBCTL_POLSEL_MASK |
                  MCPWM_DBCTL_OUTSWAP_MASK |
                  MCPWM_DBCTL_DEDB_MODE_MASK |
                  MCPWM_DBCTL_LOADREDMODE_MASK |
                  MCPWM_DBCTL_LOADFEDMODE_MASK |
                  (UINT32_C(1)<<MCPWM_DBCTL_IN_MODE_OFS);
    uint32_t expected=(1U<<DL_MCPWM_DB_RED) | (AM13E_MOTOR_DB_FED_TICKS?(1U<<DL_MCPWM_DB_FED):0U) |
          ((uint32_t)AM13E_MOTOR_DB_RED_POLARITY<<MCPWM_DBCTL_POLSEL_OFS) |
          ((uint32_t)AM13E_MOTOR_DB_FED_POLARITY<<(MCPWM_DBCTL_POLSEL_OFS+1U)) |
          ((uint32_t)AM13E_MOTOR_DB_RED_INPUT<<MCPWM_DBCTL_IN_MODE_OFS) |
          ((uint32_t)AM13E_MOTOR_DB_SWAP_A<<MCPWM_DBCTL_OUTSWAP_OFS) |
          ((uint32_t)AM13E_MOTOR_DB_SWAP_B<<(MCPWM_DBCTL_OUTSWAP_OFS+1U)) |
          ((uint32_t)DL_MCPWM_RED_LOAD_FREEZE<<MCPWM_DBCTL_LOADREDMODE_OFS) |
          ((uint32_t)DL_MCPWM_FED_LOAD_FREEZE<<MCPWM_DBCTL_LOADFEDMODE_OFS);
    if (fed_input==DL_MCPWM_DB_INPUT_DB_RED) {
        expected|=MCPWM_DBCTL_DEDB_MODE_MASK;
        /* TI DriverLib only asserts DEDB_MODE for DB_RED input; it
         * does not reset the other FED input select bit in this case.
         */
    } else {
        mask|=(UINT32_C(1)<<(MCPWM_DBCTL_IN_MODE_OFS+1U));
        expected|=fed_input<<(MCPWM_DBCTL_IN_MODE_OFS+1U);
    }
    if ((dbctl&mask)!=(expected&mask) ||
        (MOTOR_PWM->DBRED & MCPWM_DBRED_DBRED_MASK)!=AM13E_MOTOR_DB_RED_TICKS ||
        (MOTOR_PWM->DBFED & MCPWM_DBFED_DBFED_MASK)!=(AM13E_MOTOR_DB_FED_TICKS?AM13E_MOTOR_DB_FED_TICKS:1U) ||
        (MOTOR_PWM->DBREDS & MCPWM_DBREDS_DBREDS_MASK)!=AM13E_MOTOR_DB_RED_TICKS ||
        (MOTOR_PWM->DBFEDS & MCPWM_DBFEDS_DBFEDS_MASK)!=(AM13E_MOTOR_DB_FED_TICKS?AM13E_MOTOR_DB_FED_TICKS:1U))
        am13e_app_motor_fault_reset();
#else
    /* Unqualified board: do not invent a valid dead-time. Explicitly
     * disable both DB output paths as well as any output swap.
     */
    DL_MCPWM_setDeadBandDelayMode(MOTOR_PWM,DL_MCPWM_DB_RED,false);
    DL_MCPWM_setDeadBandDelayMode(MOTOR_PWM,DL_MCPWM_DB_FED,false);
    DL_MCPWM_setDeadBandOutputSwapMode(MOTOR_PWM,DL_MCPWM_DB_OUTPUT_A,false);
    DL_MCPWM_setDeadBandOutputSwapMode(MOTOR_PWM,DL_MCPWM_DB_OUTPUT_B,false);
    if ((MOTOR_PWM->DBCTL &
         (MCPWM_DBCTL_OUT_MODE_MASK|MCPWM_DBCTL_OUTSWAP_MASK))!=0U)
        am13e_app_motor_fault_reset();
#endif
}

void am13e_app_motor_init(void)
{
    /* Rel17 main() calls here after real clock/initgpio/input setup.
     * Boot must still hold PRIMASK while motor power is not qualified.
     */
    if (__get_PRIMASK() == 0U || safety_initialized || fault_latched) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    safety_initialized = 0U;
    fault_latched = 0U;
    motor_timebase_running = 0U;
    audio_owner = 0U;
    motor_owner_mode=AM13E_OWNER_IDLE;
    audio_period_valid = 0U;
    sine_entry_pending = 0U;
    sine_mode_active = 0U;
    drag_brake_aq_staged = 0U;
    lock_brake_aq_staged = 0U;
    disconnect_pwm_pads();
    /* TI SDK DL_MCPWM_init() only programs registers: it does not
     * enable SYSCTL.PWREN_MCPERIPH[PWM4].  Power must be enabled
     * before the first TBCTL RMW (DL_MCPWM_setEmulationMode).
     * This does NOT start TBCLK, connect GPIO pads, or enable PB13. */
    DL_SYSCTL_enablePower(DL_SYSCTL_PWREN_PWM4);
    if (!DL_SYSCTL_isPowerEnabled(DL_SYSCTL_PWREN_PWM4)) {
        /* Leave the bridge disconnected; do not touch an unpowered
         * MCPWM peripheral through the usual motor-fault path. */
        for (;;) { __NOP(); }
    }
    DL_MCPWM_disableTBCLK();

    DL_MCPWM_Config config;
    DL_MCPWM_initParamsSetDefault(&config);
    config.timeBaseConfig.counterMode = DL_MCPWM_COUNTER_MODE_STOP_FREEZE;
    /* TRM3.4.1: MCPWMCLK=MCLK200MHz, not MCLKDIV2/BUSCLK. */
    config.timeBaseConfig.clockDivider = DL_MCPWM_CLOCK_DIVIDER_2;
#ifdef AM13E_BOARD_SC5_CALIBRATION
    config.timeBaseConfig.syncPerSource=DL_MCPWM_SYNCPERSEL_SOURCE_ZERO;
#endif
    config.timeBaseConfig.periodValue=(uint16_t)((AM13E_APP_MCLK_HZ/2U)/24000U-1U);
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
    DL_MCPWM_init(MOTOR_PWM, &config);
    force_pwm_inactive();
    /* PB13 is initialized INACTIVE. PB15/OC GPIO inputs are prepared
     * by initgpio(); neither is installed as an MCPWM Trip source.
     */
    am13e_power_stage_init();
    configure_motor_deadband_isolated();
#ifdef SW_BLANKING
    /* Dedicated CMPC is not a phase PWM compare; CMPC/D are otherwise
     * unused by this MOTOR_PWM owner. No TIMG4/TIMG12 allocation changes. */
    DL_MCPWM_setCounterCompareShadowLoadMode(MOTOR_PWM,DL_MCPWM_COUNTER_COMPARE_C,DL_MCPWM_COMP_LOAD_ON_CNTR_ZERO);
    output_compare_shadow(MOTOR_PWM,DL_MCPWM_COUNTER_COMPARE_C,AM13E_BEMF_BLANK_TICKS);
    output_compare_active(MOTOR_PWM,DL_MCPWM_COUNTER_COMPARE_C,AM13E_BEMF_BLANK_TICKS);
    DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_1,DL_MCPWM_EVT_TBCTR_ZERO);
    DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_2,DL_MCPWM_EVT_TBCTR_U_CMPC);
    DL_MCPWM_setEventTriggerEventPrescale(MOTOR_PWM,DL_MCPWM_ET_1,1U);
    DL_MCPWM_setEventTriggerEventPrescale(MOTOR_PWM,DL_MCPWM_ET_2,1U);
    DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2);
    NVIC_SetPriority(MOTOR_PWM_IRQ,0U);
    NVIC_ClearPendingIRQ(MOTOR_PWM_IRQ);
    NVIC_EnableIRQ(MOTOR_PWM_IRQ);
#endif
    /* ZERO delivery is also used once per commutation to associate BEMF
     * with the active native image; no permanent extra carrier interrupt. */
    NVIC_SetPriority(MOTOR_PWM_IRQ,0U);
    NVIC_ClearPendingIRQ(MOTOR_PWM_IRQ);
    NVIC_EnableIRQ(MOTOR_PWM_IRQ);
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    DL_MCPWM_enableInterrupt(MOTOR_PWM,DL_MCPWM_INT_TZ_OST);
    am13e_app_motor_oc_trip_init();
    am13e_app_motor_nfault_trip_init();
#endif
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    am13e_app_motor_timing_init();
    safety_initialized = 1U; /* Inactive-preflight only, NOT motor ready. */
}

/* E1-AR real MOTOR_PWM period and six compare SHADOW writes. MCPWM TBCLK
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
        motor_transaction_end(previous_primask);
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
    DL_MCPWM_setTimeBasePeriodShadow(MOTOR_PWM, plan->period);
    for (unsigned i=0U;i<6U;++i)
        DL_MCPWM_setCounterCompareShadowValue(MOTOR_PWM,channels[i],plan->compare[i]);

    if (DL_MCPWM_getTimeBasePeriodShadow(MOTOR_PWM)!=plan->period) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    for (unsigned i=0U;i<6U;++i) {
        if (DL_MCPWM_getCounterCompareShadowValue(MOTOR_PWM,channels[i])!=plan->compare[i]) {
            am13e_app_motor_fault_shutdown();
            am13e_app_motor_fault_reset();
        }
    }
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    motor_transaction_end(previous_primask);
    return 1;
}

/* E1-AS: prepare a FULL Rel17 duty/frequency shadow image then stage
 * actual MOTOR_PWM shadow registers, but only in the previously validated
 * inactive/Hi-Z/Stop-Freeze state. Caller MUST supply board-qualified
 * dead ticks and must not treat this as enabling six-step PWM.
 */
int am13e_app_motor_stage_inactive_pwm_shadow(
    const AM13E_MotorPwmShadowInputs *input,
    AM13E_MotorShadowPlan *snapshot)
{
    if (input == NULL || input->clock_hz != (AM13E_APP_MCLK_HZ / 2U))
        return 0;
    AM13E_MotorShadowPlan plan;
    if (!am13e_motor_pwm_shadow_plan(input,&plan))
        return 0;
    if (!am13e_app_motor_stage_inactive_shadow(&plan))
        return 0;
    if (snapshot != NULL) *snapshot=plan;
    return 1;
}

/* E1-AT: stage genuine TI MOTOR_PWM AQ shadows for a candidate six-step
 * logical phase map, with all output paths still disconnected and the
 * continuously-low SW AQ forces left IN PLACE. Do not transfer AQ shadow
 * into active registers, release forced-low, start TBCLK or drive PB13.
 */
int am13e_app_motor_stage_inactive_aq_shadow(const AM13E_MotorAQShadowPlan *plan)
{
    if (!am13e_motor_aq_plan_validate(plan)) return 0;
    const uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched) {
        motor_transaction_end(saved_mask);
        return 0;
    }
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }

    static const DL_MCPWM_ACTION_QUALIFIER_MODULE modules[6] = {
        DL_MCPWM_ACTION_QUALIFIER_1A, DL_MCPWM_ACTION_QUALIFIER_1B,
        DL_MCPWM_ACTION_QUALIFIER_2A, DL_MCPWM_ACTION_QUALIFIER_2B,
        DL_MCPWM_ACTION_QUALIFIER_3A, DL_MCPWM_ACTION_QUALIFIER_3B
    };
    static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE outputs[6] = {
        DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_OUTPUT_1B,
        DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_OUTPUT_2B,
        DL_MCPWM_AQ_OUTPUT_3A, DL_MCPWM_AQ_OUTPUT_3B
    };
    for (unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,modules[i],DL_MCPWM_AQ_LOAD_FREEZE);
        DL_MCPWM_setActionQualifierActionCompleteShadow(
            MOTOR_PWM,outputs[i],plan->action[i]);
    }
    const uint32_t actual[6] = {
        MOTOR_PWM->PWM1_AQCTLAS, MOTOR_PWM->PWM1_AQCTLBS,
        MOTOR_PWM->PWM2_AQCTLAS, MOTOR_PWM->PWM2_AQCTLBS,
        MOTOR_PWM->PWM3_AQCTLAS, MOTOR_PWM->PWM3_AQCTLBS
    };
    /* The six two-bit AQ load fields are at 0,2,8,10,16,18. */
    const uint32_t freeze_fields = UINT32_C(0x000F0F0F);
    if ((MOTOR_PWM->AQCTL & freeze_fields)!=freeze_fields) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    for (unsigned i=0U;i<6U;++i)
        if (actual[i]!=(uint32_t)plan->action[i]) {
            am13e_app_motor_fault_shutdown();
            am13e_app_motor_fault_reset();
        }
    if (!pwm_pads_disconnected() || !pwm_registers_inactive()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    motor_transaction_end(saved_mask);
    return 1;
}

/* Verify the FULL (unmasked) Rel17 p/n/cc tuple before writing any AQ
 * shadow. Unsupported active-freewheeling and unknown phase coding are
 * explicitly rejected; no physical motor pins are enabled.
 */
int am13e_app_motor_stage_inactive_sixstep_aq(
    int positive_mask,int negative_mask,int comp_code,int damp,int reverse)
{
    AM13E_SixstepPlan phase;
    AM13E_MotorAQShadowPlan aq;
    if (!am13e_motor_plan_sixstep(positive_mask,negative_mask,
                                  comp_code,damp,reverse,&phase) ||
        !am13e_motor_aq_plan_sixstep(&phase,&aq))
        return 0;
    return am13e_app_motor_stage_inactive_aq_shadow(&aq);
}

/* E1-AU: FW1-startup reachable, INACTIVE AQ shadow verification.
 * All PWM pads remain GPIO Hi-Z, AQ SW force low, TBCLK stopped.
 * 12 original six-step vectors stage SHADOW registers only; then coast.
 */
static volatile uint32_t aq_boot_steps_passed;
static volatile uint32_t aq_boot_coast_restored;
int am13e_app_motor_inactive_aq_boot_preflight(void)
{
    if (__get_PRIMASK()==0U || !am13e_app_motor_inactive_preflight_ok() ||
        (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK)!=0U)
        return 0;
    static const uint16_t seq[6]={0x175U,0xD9U,0x1ABU,0x72U,0x1DEU,0xACU};
    aq_boot_steps_passed=0U;
    aq_boot_coast_restored=0U;
    const uint32_t before[6]={
        MOTOR_PWM->PWM1_AQCTLA,MOTOR_PWM->PWM1_AQCTLB,
        MOTOR_PWM->PWM2_AQCTLA,MOTOR_PWM->PWM2_AQCTLB,
        MOTOR_PWM->PWM3_AQCTLA,MOTOR_PWM->PWM3_AQCTLB
    };
    for(int reverse=0;reverse<=1;++reverse) {
        for(unsigned step=0U;step<6U;++step) {
            const uint32_t x=seq[step],mask=x>>3U;
            const int p=(int)(x&mask),n=(int)((~x)&mask);
            const int cc=(int)((mask>>3U)^(reverse?4U:0U));
            if (!am13e_app_motor_stage_inactive_sixstep_aq(
                    p,n,cc,0,reverse)) return 0;
            ++aq_boot_steps_passed;
        }
    }
    const AM13E_MotorAQShadowPlan coast={{0U,0U,0U,0U,0U,0U}};
    if (!am13e_app_motor_stage_inactive_aq_shadow(&coast)) return 0;
    const uint32_t after[6]={
        MOTOR_PWM->PWM1_AQCTLA,MOTOR_PWM->PWM1_AQCTLB,
        MOTOR_PWM->PWM2_AQCTLA,MOTOR_PWM->PWM2_AQCTLB,
        MOTOR_PWM->PWM3_AQCTLA,MOTOR_PWM->PWM3_AQCTLB
    };
    for(unsigned i=0U;i<6U;++i)
        if(after[i]!=before[i]) return 0;
    if (MOTOR_PWM->PWM1_AQCTLAS!=0U || MOTOR_PWM->PWM1_AQCTLBS!=0U ||
        MOTOR_PWM->PWM2_AQCTLAS!=0U || MOTOR_PWM->PWM2_AQCTLBS!=0U ||
        MOTOR_PWM->PWM3_AQCTLAS!=0U || MOTOR_PWM->PWM3_AQCTLBS!=0U ||
        !am13e_app_motor_inactive_preflight_ok() ||
        (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK)!=0U)
        return 0;
    aq_boot_coast_restored=1U;
    return aq_boot_steps_passed==12U;
}

/* E1-AV: Rel17 motor runtime callbacks: not a preflight or a fake HAL.
 * Logical commutation and duty changes now reach live MCPWM shadow
 * registers. AQ/compare transfers happen together at ZERO events.
 *
 * Initial board output gating is STILL separate (external gate enable
 * polarity, Trip Zone, and dead-band have not been established).
 * No callback in this block toggles GPIO output pads or PB13.
 */
static volatile uint32_t runtime_aq_updates;
static volatile uint32_t runtime_pwm_updates;
static volatile uint32_t runtime_commit_count;
static volatile uint32_t runtime_phase_pending;
static uint16_t runtime_aq_last[6];

static const DL_MCPWM_ACTION_QUALIFIER_MODULE runtime_aq_modules[6]={
    DL_MCPWM_ACTION_QUALIFIER_1A, DL_MCPWM_ACTION_QUALIFIER_1B,
    DL_MCPWM_ACTION_QUALIFIER_2A, DL_MCPWM_ACTION_QUALIFIER_2B,
    DL_MCPWM_ACTION_QUALIFIER_3A, DL_MCPWM_ACTION_QUALIFIER_3B
};
static const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE runtime_aq_outputs[6]={
    DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_OUTPUT_1B,
    DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_OUTPUT_2B,
    DL_MCPWM_AQ_OUTPUT_3A, DL_MCPWM_AQ_OUTPUT_3B
};
static const DL_MCPWM_COUNTER_COMPARE_MODULE runtime_compare_modules[6]={
    DL_MCPWM_COUNTER_COMPARE_1A, DL_MCPWM_COUNTER_COMPARE_1B,
    DL_MCPWM_COUNTER_COMPARE_2A, DL_MCPWM_COUNTER_COMPARE_2B,
    DL_MCPWM_COUNTER_COMPARE_3A, DL_MCPWM_COUNTER_COMPARE_3B
};

/* QEMU-only safety diagnostics; never disables any motor protection.
 * Stores the precise return PC at the moment the existing fail-closed
 * runtime_fault() is invoked, before shutdown/fault_latched change state.
 * This symbol has no GPIO/IRQ/PWM side effects and is never used to
 * grant motor authorization. Unset in normal firmware builds.
 */
#if defined(AM13E_E62_SIM_ONLY_MOTOR_FAULT_DIAG)
__attribute__((used)) volatile uintptr_t e62_sim_runtime_fault_caller_pc;
__attribute__((used)) volatile uint32_t e62_sim_runtime_fault_prestate;
__attribute__((noinline))
#endif
static void runtime_fault(void)
{
#if defined(AM13E_E62_SIM_ONLY_MOTOR_FAULT_DIAG)
    e62_sim_runtime_fault_caller_pc=(uintptr_t)__builtin_return_address(0);
    e62_sim_runtime_fault_prestate=
        ((audio_owner&3U)<<3U) |
        ((safety_initialized&1U)<<2U) |
        ((fault_latched&1U)<<1U) |
        (motor_timebase_running&1U);
#endif
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
}
static void owner_move(AM13E_OwnerEvent action)
{
    AM13E_MotorOwner next=AM13E_OWNER_IDLE;
    if(!am13e_motor_owner_next((AM13E_MotorOwner)motor_owner_mode,
                               action,&next)) runtime_fault();
    motor_owner_mode=next;
}

/* Called by Rel17 nextstep(). Full six-bit p/n masks and comparator
 * code are validated against the original six commutation tuples.
 * AQ shadow load is switched from startup FREEZE to synchronous ZERO:
 * this is real hardware scheduling, not debugger-only staging.
 */
void am13e_app_motor_sixstep_write(int positive_mask,int negative_mask,
                                   int floating_phase,int damp,int reverse)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (audio_owner) runtime_fault();
    AM13E_SixstepPlan phase;
    AM13E_MotorAQShadowPlan aq;
    if (!safety_initialized || fault_latched ||
        !am13e_motor_plan_sixstep(positive_mask,negative_mask,
                                  floating_phase,damp,reverse,&phase))
        runtime_fault();
    /* Preserve actual Rel17 cfg.damp and generate the complementary
     * active-freewheel AQ image ONLY with an explicit board-configured
     * hardware dead-band. Without it remain fail-closed, not downgraded.
     * Physical output requires the complete electrical board definitions.
     */
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
    if (phase.damp && (positive_mask || negative_mask)) runtime_fault();
#endif
    if (!am13e_motor_aq_plan_sixstep(&phase,&aq)) runtime_fault();

    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    owner_move(AM13E_OWNER_EVENT_SIXSTEP);
    am13e_app_motor_bemf_stage((unsigned)floating_phase);
    for(unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,runtime_aq_modules[i],DL_MCPWM_AQ_LOAD_ON_CNTR_ZERO);
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],aq.action[i]);
        runtime_aq_last[i]=aq.action[i];
    }
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
    const uint32_t readback[6]={
        MOTOR_PWM->PWM1_AQCTLAS,MOTOR_PWM->PWM1_AQCTLBS,
        MOTOR_PWM->PWM2_AQCTLAS,MOTOR_PWM->PWM2_AQCTLBS,
        MOTOR_PWM->PWM3_AQCTLAS,MOTOR_PWM->PWM3_AQCTLBS
    };
    for(unsigned i=0U;i<6U;++i)
        if(readback[i] != (uint32_t)aq.action[i])runtime_fault();
#endif
    runtime_phase_pending=1U;
    sine_entry_pending=0U;
    sine_mode_active=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    ++runtime_aq_updates;
    motor_transaction_end(primask);
}

/* Rel17 16..96 kHz ramp and logical 0..2000 duty are preserved by
 * the E1-AH/E1-AI/E1-AS planners. Duty is sent to all 6 compare
 * shadows and latched on timebase ZERO, not through an unqualified
 * fixed-duty waveform or fixed-frequency shortcut.
 */
void am13e_app_motor_pwm_apply(int duty,int freq_min_khz,int freq_max_khz,
                                int ertm_us,int damp,int lock,int brushed,
                                int running)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched)runtime_fault();
    AM13E_MotorPwmShadowInputs input={
        .clock_hz=AM13E_APP_MCLK_HZ/2U,
        .freq_min_khz=freq_min_khz,
        .freq_max_khz=freq_max_khz,
        .ertm_us=ertm_us,
        .logical_duty=duty,
#ifdef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
        .board_dead_ticks=AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS,
#else
        .board_dead_ticks=0, /* Unqualified: lock/damp remain fail-closed. */
#endif
        .lock=lock,.running=running,.damp=damp,.brushed=brushed,
#ifdef FULL_DUTY
        .full_duty=1
#else
        .full_duty=0
#endif
    };
    /* When the control requires dead-time compensation, no board
     * qualified count is available. Reject instead of applying 0ns.
     * The proper complementary/dead-band runtime remains to be ported.
     */
    if (lock<0 || lock>2) runtime_fault();
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
    /* Only board-qualified dead-band may be used for lock=1/2 or
     * active complementary freewheel. Never silently use 0ns.
     */
    if (lock || (running && damp)) runtime_fault();
#endif
    AM13E_MotorShadowPlan plan;
    if (!am13e_motor_pwm_shadow_plan(&input,&plan))runtime_fault();

    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    DL_MCPWM_setPeriodLoadMode(MOTOR_PWM,
                               DL_MCPWM_PERIOD_SHADOW_LOAD_ENABLE);
    output_period_shadow(MOTOR_PWM,plan.period);
    for(unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setCounterCompareShadowLoadMode(
            MOTOR_PWM,runtime_compare_modules[i],
            DL_MCPWM_COMP_LOAD_ON_CNTR_ZERO);
        output_compare_shadow(
            MOTOR_PWM,runtime_compare_modules[i],plan.compare[i]);
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
        if (DL_MCPWM_getCounterCompareShadowValue(
                MOTOR_PWM,runtime_compare_modules[i])!=plan.compare[i])
            runtime_fault();
#endif
    }
    if(DL_MCPWM_getTimeBasePeriodShadow(MOTOR_PWM)!=plan.period)
        runtime_fault();
    /* A counter-zero shadow load cannot be relied on while TBCLK is
     * stopped. On the initial/restart path, seed the ACTIVE period and
     * compare bank too, but ONLY while software-forced LOW and the six
     * physical pads are still GPIO inputs. A live PWM update must use
     * the normal ZERO-event shadow path instead of writing ACTIVE.
     * This closes the first-cycle startup register-image gap without
     * treating a prepared timebase as an authorized power-stage enable.
     */
    if ((MOTOR_PWM->TBCTL & MCPWM_TBCTL_CTRMODE_MASK)==
          (uint32_t)DL_MCPWM_COUNTER_MODE_STOP_FREEZE) {
        if (!pwm_registers_inactive() || !pwm_pads_disconnected())
            runtime_fault();
        output_period_active(MOTOR_PWM,plan.period);
        for(unsigned i=0U;i<6U;++i) {
            output_compare_active(
                MOTOR_PWM,runtime_compare_modules[i],plan.compare[i]);
            if (DL_MCPWM_getCounterCompareActiveValue(
                    MOTOR_PWM,runtime_compare_modules[i])!=plan.compare[i])
                runtime_fault();
        }
        if (DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM)!=plan.period)
            runtime_fault();
    }
    ++runtime_pwm_updates;
    motor_transaction_end(primask);
}

/* Rel17 brushed start: forward drives phases U/W with PWM and uses
 * V as the static return leg; reverse PWM-drives V and returns via U/W.
 * This is the original non-PWM_ENABLE, non-damped six-output mapping
 * from src/main.c, not a substitute six-step commutation sequence.
 *
 * AQ is scheduled at ZERO and committed on a stopped timebase through
 * the normal Rel17 commit path. Gate pads and software force remain
 * physically inactive until the separate, board-qualified enable path.
 * Active freewheel/braking with damp requires verified complementary
 * gates and dead-band. Until then, reject it rather than synthesizing
 * unsafe overlap or silently downgrading requested behavior.
 */
void am13e_app_motor_brushed_write(int reverse,int damp)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched ||
        (reverse != 0 && reverse != 1) || (damp != 0 && damp != 1))
        runtime_fault();
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
    if (damp) runtime_fault();
#endif
    const uint16_t pwm = (uint16_t)(DL_MCPWM_AQ_OUTPUT_HIGH_ZERO |
                                    DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA);
    const uint16_t sink = (uint16_t)DL_MCPWM_AQ_OUTPUT_HIGH_ZERO;
    const uint16_t freewheel = (uint16_t)(DL_MCPWM_AQ_OUTPUT_LOW_ZERO |
                                          DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
    const uint16_t action[6] = {
        reverse ? 0U : pwm, reverse ? sink : (damp ? freewheel : 0U),
        reverse ? pwm : 0U, reverse ? (damp ? freewheel : 0U) : sink,
        reverse ? 0U : pwm, reverse ? sink : (damp ? freewheel : 0U)
    };
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    owner_move(AM13E_OWNER_EVENT_SIXSTEP);
    for(unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,runtime_aq_modules[i],DL_MCPWM_AQ_LOAD_ON_CNTR_ZERO);
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],action[i]);
        runtime_aq_last[i]=action[i];
    }
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
    const uint32_t readback[6]={
        MOTOR_PWM->PWM1_AQCTLAS,MOTOR_PWM->PWM1_AQCTLBS,
        MOTOR_PWM->PWM2_AQCTLAS,MOTOR_PWM->PWM2_AQCTLBS,
        MOTOR_PWM->PWM3_AQCTLAS,MOTOR_PWM->PWM3_AQCTLBS
    };
    for(unsigned i=0U;i<6U;++i)
        if(readback[i] != (uint32_t)action[i])runtime_fault();
#endif
    runtime_phase_pending=1U;
    sine_entry_pending=0U;
    sine_mode_active=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    ++runtime_aq_updates;
    motor_transaction_end(primask);
    am13e_app_motor_commutation_commit();
}

/* Rel17 main.c::nextstep() sine path: index a/b/c already includes
 * direction and +/-120 degree offsets.  The original waveform comes
 * directly from STM32G431/preset.c and uses (sinedata[idx]*power)>>7,
 * on a fixed ~24 kHz carrier, independent of six-step frequency ramp.
 *
 * Compare uses the original modulation plus the explicit board dead-time
 * offset. The configured electrical adapter drives separate A/B AQ paths;
 * an unconfigured Reference keeps software force and GPIO isolation.
 */
void am13e_app_motor_sine_write(int a,int b,int c,int power,int start)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched ||
        (unsigned)a>=360U || (unsigned)b>=360U || (unsigned)c>=360U ||
        power<0 || power>120 || (start!=0 && start!=1) ||
        (start && (sine_mode_active || sine_entry_pending)) ||
        (!start && !sine_mode_active))
        runtime_fault();
    /* Rel17 STM32G431: 168MHz TIM1, carrier 24kHz, period 7000
     * timer counts. Its sine LUT * power >> 7 is a RAW TIMER COUNT,
     * not a normalized 0..2000 duty. Retaining that count on 100MHz
     * AM13E would increase modulation duty by ~1.68x.
     */
    enum { SINE_CARRIER_HZ = 24000, REL17_TIM1_HZ = 168000000 };
    const uint32_t old_ticks=REL17_TIM1_HZ/SINE_CARRIER_HZ;
    const uint32_t ticks=(AM13E_APP_MCLK_HZ/2U)/SINE_CARRIER_HZ;
    _Static_assert(REL17_TIM1_HZ/SINE_CARRIER_HZ==7000U,
                   "Rel17 STM32G431 sine carrier contract changed");
    if (ticks<=2U || ticks>UINT16_MAX) runtime_fault();
    _Static_assert(((UINT64_C(65535)*120U)>>7U)*
        ((AM13E_APP_MCLK_HZ/2U)/SINE_CARRIER_HZ)+3500U<=UINT32_MAX,
        "Sine timer conversion numerator exceeds 32-bit range");
    const uint16_t period=(uint16_t)(ticks-1U);
    const int idx[3]={a,b,c};
    uint16_t wave[3];
    for(unsigned i=0U;i<3U;++i) {
        const uint32_t rel17_count=((uint32_t)sinedata[idx[i]]*
                                     (uint32_t)power)>>7U;
        /* Scale the fraction of the original carrier period, with
         * one-tick rounding and no floating point in the motor IRQ.
         */
        uint32_t compare=(uint32_t)((rel17_count*ticks+
                                      old_ticks/2U)/old_ticks);
#ifdef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
        /* Only a verified board can supply the original DEAD_TIME-like
         * compare offset. Never infer it from a legacy MCU register.
         */
        compare+=AM13E_MOTOR_DB_COMPARE_OFFSET_TICKS;
#endif
        if (compare>=ticks) runtime_fault();
        wave[i]=(uint16_t)compare;
    }
    const uint16_t aq_a=(uint16_t)(
        DL_MCPWM_AQ_OUTPUT_HIGH_ZERO|DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA);
    const uint16_t aq_b=(uint16_t)(
        DL_MCPWM_AQ_OUTPUT_LOW_ZERO|DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    owner_move(AM13E_OWNER_EVENT_SINE);
    if (!pwm_io_for_runtime()) runtime_fault();
    const int frozen=(MOTOR_PWM->TBCTL&MCPWM_TBCTL_CTRMODE_MASK)==
                       (uint32_t)DL_MCPWM_COUNTER_MODE_STOP_FREEZE;
    if (frozen && !pwm_registers_inactive()) runtime_fault();
    DL_MCPWM_setPeriodLoadMode(MOTOR_PWM,DL_MCPWM_PERIOD_SHADOW_LOAD_ENABLE);
    output_period_shadow(MOTOR_PWM,period);
    for(unsigned i=0U;i<6U;++i) {
        const uint16_t compare=wave[i/2U];
        const uint16_t action=(i&1U)?aq_b:aq_a;
        DL_MCPWM_setCounterCompareShadowLoadMode(
            MOTOR_PWM,runtime_compare_modules[i],DL_MCPWM_COMP_LOAD_ON_CNTR_ZERO);
        output_compare_shadow(
            MOTOR_PWM,runtime_compare_modules[i],compare);
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,runtime_aq_modules[i],DL_MCPWM_AQ_LOAD_ON_CNTR_ZERO);
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],action);
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
        if (DL_MCPWM_getCounterCompareShadowValue(
                MOTOR_PWM,runtime_compare_modules[i])!=compare)
            runtime_fault();
#endif
        runtime_aq_last[i]=action;
        if (frozen) {
            output_compare_active(
                MOTOR_PWM,runtime_compare_modules[i],compare);
            output_aq_active(
                MOTOR_PWM,runtime_aq_outputs[i],action);
            if (DL_MCPWM_getCounterCompareActiveValue(
                    MOTOR_PWM,runtime_compare_modules[i])!=compare)
                runtime_fault();
        }
    }
    /* One coherent AQ Shadow snapshot, rather than six 6-register MMIO
     * snapshots on every Rel17 high-rate sine commutation event.
     */
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
    const uint32_t aq_shadow[6]={
        MOTOR_PWM->PWM1_AQCTLAS,MOTOR_PWM->PWM1_AQCTLBS,
        MOTOR_PWM->PWM2_AQCTLAS,MOTOR_PWM->PWM2_AQCTLBS,
        MOTOR_PWM->PWM3_AQCTLAS,MOTOR_PWM->PWM3_AQCTLBS
    };
    for(unsigned i=0U;i<6U;++i)
        if (aq_shadow[i]!=(uint32_t)((i&1U)?aq_b:aq_a))
            runtime_fault();
#endif
    if (frozen) output_period_active(MOTOR_PWM,period);
    if (DL_MCPWM_getTimeBasePeriodShadow(MOTOR_PWM)!=period ||
        (frozen && DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM)!=period))
        runtime_fault();
    /* Sine image supersedes any prior six-step COM pending flag. */
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    runtime_phase_pending=0U;
    ++sine_write_count;
    if (start) sine_entry_pending=1U;
    motor_transaction_end(irqmask);
}

/* Rel17 nextstep() calls sine_finish() ONLY ON FIRST SINE SAMPLE:
 * after programming the original three CCRs, STM32 initializes PWM
 * mode, output topology and comparator-off state.  It is NOT the
 * sine->sixstep exit callback. Keep TIMG12's freshly scheduled next
 * sine step intact; on AM13E, stop() or subsequent sixstep_write()
 * terminates the waveform. Power-stage output remains isolated.
 */
void am13e_app_motor_sine_finish(void)
{
    if (audio_owner) runtime_fault();
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched ||
        !sine_entry_pending || sine_mode_active ||
        !pwm_io_for_runtime()) runtime_fault();
    sine_entry_pending=0U;
    sine_mode_active=1U;
    motor_transaction_end(irqmask);
}

/* Rel17 laststep(), start boundary: AQ shadow-to-active transfers are
 * scheduled on ZERO, on ALL six channels together. When the motor
 * timebase is already frozen, explicitly write the same AQ active
 * configuration so the next eventual enable starts from a coherent
 * image. Software force LOW and external pads remain unchanged.
 */
void am13e_app_motor_commutation_commit(void)
{
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched)runtime_fault();
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if (runtime_phase_pending &&
        (MOTOR_PWM->TBCTL & MCPWM_TBCTL_CTRMODE_MASK)==
         (uint32_t)DL_MCPWM_COUNTER_MODE_STOP_FREEZE) {
        for(unsigned i=0U;i<6U;++i)
            output_aq_active(
                MOTOR_PWM,runtime_aq_outputs[i],runtime_aq_last[i]);
    }
    runtime_phase_pending=0U;
    ++runtime_commit_count;
    motor_transaction_end(primask);
}

/* Original Core run/stop lifecycle. A configured board connects the six
 * MCPWM pads, releases continuous force and enables PB13 after carrier
 * startup. Stop/fault deassert the driver first. Brake/lock are restarted
 * by the original laststep/setduty hooks, not replaced by permanent Hi-Z. */
void am13e_app_motor_commutation_enable(int enable)
{
    if (audio_owner) runtime_fault();
    const uint32_t irqmask=__get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched ||
        (enable != 0 && enable != 1)) runtime_fault();
    owner_move(enable?AM13E_OWNER_EVENT_MOTOR_START:
                      AM13E_OWNER_EVENT_MOTOR_STOP);
    if (enable == 0) {
        sine_entry_pending=0U;
        sine_mode_active=0U;
        am13e_app_motor_timing_cancel(); /* Abort sine/commutation IRQ. */
        am13e_app_motor_bemf_abort(); /* Abort ECAP1 zero-cross IRQ. */
        force_pwm_inactive();       /* Stops shared TBCLK and freezes MOTOR_PWM. */
        disconnect_pwm_pads();
        motor_timebase_running=0U;
        ++motor_timebase_stops;
        if (!pwm_registers_inactive() || !pwm_pads_disconnected() ||
            (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
            runtime_fault();
    } else {
        /* Core requested motor or active brake operation. The unconfigured
         * Reference stays isolated; an electrical board build attaches PWM. */
        if (!pwm_io_for_runtime())
            runtime_fault();
        if (!motor_timebase_running) {
            if (!pwm_registers_inactive() ||
                (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
                runtime_fault();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
            output_commit_shadow();
            if(!am13e_motor_output_start())runtime_fault();
#endif
            DL_MCPWM_setTimeBaseCounter(MOTOR_PWM,0U);
            DL_MCPWM_setTimeBaseCounterMode(MOTOR_PWM,
                                            DL_MCPWM_COUNTER_MODE_UP);
            DL_MCPWM_enableTBCLK();
            if ((MOTOR_PWM->TBCTL & MCPWM_TBCTL_CTRMODE_MASK) !=
                   (uint32_t)DL_MCPWM_COUNTER_MODE_UP ||
                !(SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
                runtime_fault();
            motor_timebase_running=1U;
            ++motor_timebase_starts;
        } else if ((MOTOR_PWM->TBCTL & MCPWM_TBCTL_CTRMODE_MASK) !=
                     (uint32_t)DL_MCPWM_COUNTER_MODE_UP ||
                   !(SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
            runtime_fault();
        /* Carrier startup ISR handles the configured driver's enable. */
    }
    motor_transaction_end(irqmask);
}


/* Rel17 Motor Audio borrows MOTOR_PWM, never a separate beeper GPIO.
 * A configured board connects the same six output pads for audio.
 * The unconfigured Reference remains isolated; no electrical values are guessed.
 */
static void audio_stage_compare_locked(uint16_t u, uint16_t w,int active)
{
    const uint16_t pwm=(uint16_t)(DL_MCPWM_AQ_OUTPUT_HIGH_ZERO |
                                  DL_MCPWM_AQ_OUTPUT_LOW_UP_CMPA);
    const uint16_t inverse=(uint16_t)(DL_MCPWM_AQ_OUTPUT_LOW_ZERO |
                                      DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
    const uint16_t sink=(uint16_t)DL_MCPWM_AQ_OUTPUT_HIGH_ZERO;
#ifdef PWM_ENABLE
    /* Original util.c enables all three bridge phases even at a silent
     * zero compare. PWM U/W alternate; V remains the static return. */
    const uint16_t wave[6]={pwm,inverse,0U,sink,pwm,inverse};
#else
    const uint16_t wave[6]={u?pwm:0U,u?inverse:0U,0U,
                             (u||w)?sink:0U,w?pwm:0U,w?inverse:0U};
#endif
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    /* Audio formatters supply modulation counts without the hardware DT.
     * Preserve pulse width after RED at the actual selected clock divider. */
    const uint16_t dead=(uint16_t)(MOTOR_PWM->DBRED&MCPWM_DBRED_DBRED_MASK);
    if((uint32_t)u+dead>UINT16_MAX || (uint32_t)w+dead>UINT16_MAX)runtime_fault();
    const uint16_t cu=u?(uint16_t)(u+dead):0U,cw=w?(uint16_t)(w+dead):0U;
    const uint16_t compare[6]={cu,cu,0U,0U,cw,cw};
#else
    const uint16_t compare[6]={u,u,0U,0U,w,w};
#endif
    for (unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,runtime_aq_modules[i],DL_MCPWM_AQ_LOAD_ON_CNTR_ZERO);
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],wave[i]);
        DL_MCPWM_setCounterCompareShadowLoadMode(
            MOTOR_PWM,runtime_compare_modules[i],DL_MCPWM_COMP_LOAD_ON_CNTR_ZERO);
        output_compare_shadow(
            MOTOR_PWM,runtime_compare_modules[i],compare[i]);
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
        if (DL_MCPWM_getCounterCompareShadowValue(
            MOTOR_PWM,runtime_compare_modules[i])!=compare[i])
            runtime_fault();
#endif
        if(active){
            output_compare_active(
                MOTOR_PWM,runtime_compare_modules[i],compare[i]);
            output_aq_active(
                MOTOR_PWM,runtime_aq_outputs[i],wave[i]);
        }
    }
}

/* Must be called only after Rel17 util.c::resetcom() and its ertm/busy
 * admission check. Drive callbacks reject audio_owner from this point.
 * Preserve PB14 RX/TIMG4, PB15 OST and the 16kHz SysTick.
 */
void am13e_app_motor_audio_begin(int mode)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (mode!=1 && mode!=2) runtime_fault();
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched || audio_owner ||
        motor_timebase_running || !pwm_registers_inactive() ||
        !pwm_pads_disconnected() ||
        (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
        runtime_fault();
    owner_move(mode==1?AM13E_OWNER_EVENT_MUSIC_START:
                       AM13E_OWNER_EVENT_PCM_START);
    am13e_app_motor_timing_cancel();
    am13e_app_motor_bemf_abort();
    sine_mode_active=0U;
    sine_entry_pending=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    runtime_phase_pending=0U;
    audio_owner=(uint32_t)mode;
    audio_period_valid=0U;
    DL_MCPWM_setClockPrescaler(MOTOR_PWM,
         mode==1?DL_MCPWM_CLOCK_DIVIDER_32:DL_MCPWM_CLOCK_DIVIDER_2);
    if ((MOTOR_PWM->TBCTL & MCPWM_TBCTL_CLKDIV_MASK) !=
       (uint32_t)(mode==1?DL_MCPWM_CLOCK_DIVIDER_32:
                  DL_MCPWM_CLOCK_DIVIDER_2)<<MCPWM_TBCTL_CLKDIV_OFS)
        runtime_fault();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    const uint16_t audio_red=(uint16_t)(mode==1 ?
        (AM13E_MOTOR_OUTPUT_RED_TICKS+15U)/16U:AM13E_MOTOR_OUTPUT_RED_TICKS);
    DL_MCPWM_setRisingEdgeDelayCountActive(MOTOR_PWM,audio_red);
    DL_MCPWM_setRisingEdgeDelayCountShadow(MOTOR_PWM,audio_red);
#ifdef PWM_ENABLE
    DL_MCPWM_setFallingEdgeDelayCountActive(MOTOR_PWM,audio_red);
    DL_MCPWM_setFallingEdgeDelayCountShadow(MOTOR_PWM,audio_red);
#endif
#endif
    /* ET2 is loaned by BEMF blanking to music's sticky period flag.
     * ET2 stays latched until the original delayf poll consumes it. */
    DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_2,
        mode==1?DL_MCPWM_EVT_TBCTR_ZERO:DL_MCPWM_EVT_DISABLED);
    DL_MCPWM_setEventTriggerEventPrescale(MOTOR_PWM,DL_MCPWM_ET_2,1U);
    audio_stage_compare_locked(0U,0U,1);
    for (unsigned i=0U;i<6U;++i) runtime_aq_last[i]=0U;
    motor_transaction_end(mask);
}

void am13e_app_motor_audio_period(uint16_t period)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (!audio_owner || period<2U || fault_latched ||
        !pwm_io_for_runtime()) runtime_fault();
    /* A note change is a genuine motor-timebase mode change. Stop,
     * reset phase and program both shadow and active registers before
     * the next period. No previous DRIVE/PCM waveform can leak through.
     */
    force_pwm_inactive();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    disconnect_pwm_pads();
#endif
    motor_timebase_running=0U;
    DL_MCPWM_setPeriodLoadMode(MOTOR_PWM,DL_MCPWM_PERIOD_SHADOW_LOAD_ENABLE);
    output_period_shadow(MOTOR_PWM,period);
    output_period_active(MOTOR_PWM,period);
    DL_MCPWM_setTimeBaseCounter(MOTOR_PWM,0U);
    audio_stage_compare_locked(0U,0U,1);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    audio_period_valid=1U;
    if (DL_MCPWM_getTimeBasePeriodShadow(MOTOR_PWM)!=period ||
        DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM)!=period ||
        !pwm_registers_inactive()) runtime_fault();
    motor_transaction_end(mask);
}

void am13e_app_motor_audio_compare(uint16_t u,uint16_t w)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (!audio_owner || !audio_period_valid || fault_latched ||
        !pwm_io_for_runtime() ||
        (audio_owner!=1U && (u >= DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM) ||
                            w >= DL_MCPWM_getTimeBasePeriodActive(MOTOR_PWM))))
        runtime_fault();
    const int frozen=!motor_timebase_running;
    if (frozen && !pwm_registers_inactive()) runtime_fault();
    audio_stage_compare_locked(u,w,frozen);
    if(frozen){
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        output_commit_shadow();
        if(!am13e_motor_output_start())runtime_fault();
#endif
        DL_MCPWM_setTimeBaseCounter(MOTOR_PWM,0U);
        DL_MCPWM_setTimeBaseCounterMode(MOTOR_PWM,DL_MCPWM_COUNTER_MODE_UP);
        DL_MCPWM_enableTBCLK();
        motor_timebase_running=1U;
        if(audio_owner==1U){
            NVIC_SetPriority(MOTOR_PWM_IRQ,0U);
            NVIC_EnableIRQ(MOTOR_PWM_IRQ);
            DL_MCPWM_enableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        }
        if (!pwm_io_for_runtime() ||
            !(SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
            runtime_fault();
    }
    /* Configured music/PCM uses MOTOR_PWM pins and the same gate lifecycle. */
    motor_transaction_end(mask);
}

int am13e_app_motor_audio_period_elapsed(void)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if(audio_owner!=1U || !motor_timebase_running || fault_latched)runtime_fault();
    const int elapsed=(DL_MCPWM_getInterruptSource(MOTOR_PWM)&DL_MCPWM_INT_ET_2)!=0U;
    if(elapsed){
        DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        /* TRM26.9 queues one further event while the flag is latched.
         * Rel17 UIF instead collapses all old periods into one poll.
         * Force a masked ET2 event to drain its event counter (TRM Table26-42:
         * INTFLAG generation clears ETCNT irrespective of INTEN), then
         * discard that synthetic flag before accepting a new period.
         * Do not write the read-only ETCNT register. */
        DL_MCPWM_forceInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        __DSB();
        DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        DL_MCPWM_enableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    }
    /* Do not acknowledge the global interrupt: a simultaneous OST must
     * still invoke shutdown. Only one ET2 IRQ occurs per polling interval. */
    motor_transaction_end(mask);
    return elapsed;
}

uint16_t am13e_app_motor_audio_counter(void)
{
    if(!audio_owner || !motor_timebase_running) runtime_fault();
    return DL_MCPWM_getTimeBaseCounterValue(MOTOR_PWM);
}

void am13e_app_motor_audio_end(void)
{
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (!audio_owner || !safety_initialized || fault_latched)
        runtime_fault();
    if ((audio_owner==1 && motor_owner_mode!=AM13E_OWNER_MUSIC) ||
        (audio_owner==2 && motor_owner_mode!=AM13E_OWNER_PCM))
        runtime_fault();
    owner_move(AM13E_OWNER_EVENT_AUDIO_END);
    force_pwm_inactive();
    disconnect_pwm_pads();
    motor_timebase_running=0U;
    /* Restore the inactive DRIVE staging default: divider 2,
     * 24kHz carrier, zero compare/AQ in both active and shadow banks.
     * The normal six-step PWM Apply will seed exact running settings.
     */
    DL_MCPWM_setClockPrescaler(MOTOR_PWM,DL_MCPWM_CLOCK_DIVIDER_2);
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    DL_MCPWM_setRisingEdgeDelayCountActive(MOTOR_PWM,AM13E_MOTOR_OUTPUT_RED_TICKS);
    DL_MCPWM_setRisingEdgeDelayCountShadow(MOTOR_PWM,AM13E_MOTOR_OUTPUT_RED_TICKS);
#ifdef PWM_ENABLE
    DL_MCPWM_setFallingEdgeDelayCountActive(MOTOR_PWM,AM13E_MOTOR_OUTPUT_FED_TICKS);
    DL_MCPWM_setFallingEdgeDelayCountShadow(MOTOR_PWM,AM13E_MOTOR_OUTPUT_FED_TICKS);
#endif
#endif
    DL_MCPWM_setPeriodLoadMode(MOTOR_PWM,DL_MCPWM_PERIOD_SHADOW_LOAD_ENABLE);
    const uint16_t period=(uint16_t)
        ((AM13E_APP_MCLK_HZ/2U)/UINT32_C(24000)-1U);
    output_period_shadow(MOTOR_PWM,period);
    output_period_active(MOTOR_PWM,period);
    audio_stage_compare_locked(0U,0U,1);
    for(unsigned i=0U;i<6U;++i) runtime_aq_last[i]=0U;
    runtime_phase_pending=0U;
    DL_MCPWM_setTimeBaseCounter(MOTOR_PWM,0U);
    audio_period_valid=0U;
    audio_owner=0U;
    DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    DL_MCPWM_clearInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
    DL_MCPWM_setEventTriggerSource(MOTOR_PWM,DL_MCPWM_ET_2,
#ifdef SW_BLANKING
        DL_MCPWM_EVT_TBCTR_U_CMPC);
#else
        DL_MCPWM_EVT_DISABLED);
#endif
    if (!pwm_registers_inactive() || !pwm_pads_disconnected() ||
        (SYSCTL->SOCLOCK.PERCLKCR & SYSCTL_PERCLKCR_TBCLKSYNC_MASK))
        runtime_fault();
    motor_transaction_end(mask);
}

/* Rel17 laststep() with lock==0 switches all three TIM1 channels
 * to PWM1 while the complementary N gates remain selected. Mirror
 * that three-low-side brake AQ image; the configured adapter drives
 * physical pins when Core starts braking. Compare/duty is driven by
 * Rel17 setduty -> am13e_app_motor_pwm_apply().
 *
 * This prepares Drag and Proportional Braking images; it does NOT arm
 * the bridge, assume driver polarity, or bypass overcurrent protection.
 */
void am13e_app_motor_drag_brake_write(void)
{
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    physical_runtime_image=1U;
#endif
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched) runtime_fault();
    AM13E_MotorAQShadowPlan aq;
    if (!am13e_motor_aq_plan_drag_brake(&aq)) runtime_fault();
    am13e_app_motor_sixstep_idle(); /* stop TIMG/MCPWM and isolate pads */
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    owner_move(AM13E_OWNER_EVENT_BRAKE);
    if (!pwm_registers_inactive() || !pwm_pads_disconnected())
        runtime_fault();
    for (unsigned i=0U;i<6U;++i) {
        DL_MCPWM_setActionQualifierShadowLoadMode(
            MOTOR_PWM,runtime_aq_modules[i],DL_MCPWM_AQ_LOAD_ON_CNTR_ZERO);
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],aq.action[i]);
        output_aq_active(
            MOTOR_PWM,runtime_aq_outputs[i],aq.action[i]);
        runtime_aq_last[i]=aq.action[i];
    }
#if !AM13E_MOTOR_OUTPUT_CONFIGURED
    const uint32_t readback[6]={
        MOTOR_PWM->PWM1_AQCTLAS,MOTOR_PWM->PWM1_AQCTLBS,
        MOTOR_PWM->PWM2_AQCTLAS,MOTOR_PWM->PWM2_AQCTLBS,
        MOTOR_PWM->PWM3_AQCTLAS,MOTOR_PWM->PWM3_AQCTLBS
    };
    for(unsigned i=0U;i<6U;++i)
        if(readback[i]!=(uint32_t)aq.action[i]) runtime_fault();
#endif
    runtime_phase_pending=1U;
    drag_brake_aq_staged=1U;
    lock_brake_aq_staged=0U;
    ++runtime_aq_updates;
    motor_transaction_end(primask);
}

/* Rel17 laststep() selects the lock-hold six-step image through its own
 * nextstep() before clearing the shared step to zero. Retain that
 * ownership explicitly; a stationary motor is not a normal commutation.
 * cfg.throt_ztc may intentionally make a zero-drive AQ image.
 */
void am13e_app_motor_lock_brake_stage(int lock,int phase_step)
{
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched || lock<1 || lock>2 ||
        phase_step<1 || phase_step>6 || !runtime_phase_pending ||
        !pwm_registers_inactive() || !pwm_pads_disconnected())
        runtime_fault();
    AM13E_MotorAQShadowPlan aq;
    unsigned active=0U;
    for(unsigned i=0U;i<6U;++i) {
        aq.action[i]=runtime_aq_last[i];
        active+=aq.action[i]!=0U;
    }
    if (!am13e_motor_aq_plan_validate(&aq)) runtime_fault();
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=(active!=0U); /* ZTC coast never starts hold. */
    owner_move(AM13E_OWNER_EVENT_BRAKE);
}

/* Original Rel17 setduty stage: control MOTOR_PWM internal counter for
 * Drag/Proportional (lock=0) and Lock Hold (lock=1/2), including the
 * configured physical output/gate lifecycle.
 * Motor-running and brushed startup retain their own counter owner.
 */
void am13e_app_motor_brake_counter_update(int running,int step,
                                           int brushed,int lock,int duty)
{
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched || duty<0 || duty>2000 ||
        lock<0 || lock>2 || (running!=0 && running!=1) ||
        (brushed!=0 && brushed!=1))
        runtime_fault();
    if (running || step || brushed) return;
    if (!pwm_io_for_runtime()) runtime_fault();
    if (lock) {
        if (!lock_brake_aq_staged) {
            /* Rel17 zero-throttle coast instead of lock drive. */
            if (motor_timebase_running) am13e_app_motor_commutation_enable(0);
            return;
        }
        if (drag_brake_aq_staged) runtime_fault();
#ifndef AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED
        runtime_fault(); /* Lock compare offset has no verified dead-time. */
#endif
    } else {
        if (!drag_brake_aq_staged || lock_brake_aq_staged) runtime_fault();
        const uint16_t expected=(uint16_t)(
            DL_MCPWM_AQ_OUTPUT_LOW_ZERO|DL_MCPWM_AQ_OUTPUT_HIGH_UP_CMPB);
        for(unsigned i=0U;i<6U;++i)
            if(runtime_aq_last[i]!=((i&1U)?expected:0U))runtime_fault();
    }
    if (duty>0)
        am13e_app_motor_commutation_enable(1);
    else if (motor_timebase_running)
        am13e_app_motor_commutation_enable(0);
}

/* A real coast transition for Rel17 laststep(), not an empty callback.
 * Hold all six outputs LOW and return the six AQ shadow/active words
 * to zero. Never clear a hardware fault or energize a bridge here.
 */
void am13e_app_motor_sixstep_idle(void)
{
    if (audio_owner) runtime_fault();
    if (!safety_initialized || fault_latched)runtime_fault();
    const uint32_t primask=__get_PRIMASK();
    __disable_irq();
    /* Direct and indirect transitions into Coast/Drag Brake must both
     * invalidate any previously scheduled TIMG12 commutation IRQ.
     * Do not rely on every caller having passed through resetcom().
     */
    am13e_app_motor_timing_cancel();
    am13e_app_motor_bemf_abort();
    force_pwm_inactive();
    disconnect_pwm_pads();
    motor_timebase_running=0U;
    owner_move(AM13E_OWNER_EVENT_MOTOR_STOP);
    sine_entry_pending=0U;
    sine_mode_active=0U;
    for(unsigned i=0U;i<6U;++i) {
        output_aq_shadow(
            MOTOR_PWM,runtime_aq_outputs[i],0U);
        output_aq_active(
            MOTOR_PWM,runtime_aq_outputs[i],0U);
        runtime_aq_last[i]=0U;
    }
    runtime_phase_pending=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    if(!pwm_registers_inactive() || !pwm_pads_disconnected())
        runtime_fault();
    motor_transaction_end(primask);
}

/* Rel17 util.c::resetcom(): restore *physical MCU-side inactive bridge*
 * around score/PCM playback. Never clear hardware Trip Zone or fault
 * latch, never assume PB13 gate enable polarity, and never substitute
 * a Software AQ command for the independent overcurrent hardware trip.
 *
 * Real output writes plus read-back, not a dummy link-only callback.
 * Audio borrows the same native output and timing ownership.
 */
void am13e_app_commutation_reset(void)
{
    if (audio_owner) runtime_fault(); /* resetcom before begin/after end */
    const uint32_t irqmask = __get_PRIMASK();
    __disable_irq();
    if (!safety_initialized || fault_latched) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    am13e_app_motor_timing_cancel();
    am13e_app_motor_bemf_abort();
    force_pwm_inactive();
    disconnect_pwm_pads();
    motor_timebase_running=0U;
    owner_move(AM13E_OWNER_EVENT_MOTOR_STOP);
    sine_entry_pending=0U;
    sine_mode_active=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    if (!pwm_registers_inactive() || !pwm_pads_disconnected()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
    motor_transaction_end(irqmask);
}

void am13e_app_motor_fault_shutdown(void)
{
    /* This is a real software shutdown fallback; MCPWM HW Trip Zone
     * independently overrides PWM when PB15 is configured. Dedicated
     * overcurrent routing/threshold remains a board integration gap.
     * Unknown gate-enable polarity means this does not establish
     * hardware gate-off proof and cannot authorize motor operation.
     */
    __disable_irq();
    fault_shutdown_in_progress=1U;
    fault_latched = 1U;
    audio_owner=0U;
    motor_owner_mode=AM13E_OWNER_IDLE; /* Fault dominates all owners. */
    audio_period_valid=0U;
    sine_entry_pending=0U;
    sine_mode_active=0U;
    drag_brake_aq_staged=0U;
    lock_brake_aq_staged=0U;
    am13e_app_motor_timing_cancel();
    am13e_app_motor_bemf_fault_stop();
    force_pwm_inactive();
    disconnect_pwm_pads();
    motor_timebase_running=0U;
    fault_shutdown_in_progress=0U;
}

/* Strong AM13E230x MOTOR_PWM startup vector. Only the actual hardware
 * Trip/overcurrent routing can enforce bounded-latency shutdown without
 * waiting for this CPU interrupt. Unexpected MCPWM interrupts also fault.
 */
static void motor_irq_ack(uint16_t flags)
{
    DL_MCPWM_clearInterrupt(MOTOR_PWM,flags);
    DL_MCPWM_clearGlobalInterrupt(MOTOR_PWM);
    /* Catch an asynchronous trip arriving between the ISR's snapshot
     * and acknowledgement. Never clear OST, including this late case. */
    last_trip_zone_flags|=DL_MCPWM_getTripZoneFlagStatus(MOTOR_PWM);
    if(last_trip_zone_flags){am13e_app_motor_fault_shutdown();am13e_app_motor_fault_reset();}
}
void MCPWM4_IRQHandler(void)
{
    /* Polled music ET2 flags are raw status, not enabled CPU sources. */
    last_trip_irq_flags = DL_MCPWM_getInterruptSource(MOTOR_PWM)&MOTOR_PWM->INTEN;
    last_trip_zone_flags = DL_MCPWM_getTripZoneFlagStatus(MOTOR_PWM);
    if(!last_trip_irq_flags && !last_trip_zone_flags && !fault_latched){
        /* A cleared old vector may arrive after a new start/rearm. */
        motor_irq_ack(0U);
        return;
    }
    if(audio_owner==1U && !last_trip_zone_flags && !fault_latched &&
       !(last_trip_irq_flags&~(DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2))){
        /* TRM26.9: disabled ET interrupts can hold the event counter
         * without generating INTFLAG. Generate one enabled ET2 event,
         * then mask it and preserve its sticky flag for delayf().
         * Do not run an IRQ on every high-frequency music period. */
        if(last_trip_irq_flags&DL_MCPWM_INT_ET_2)
            DL_MCPWM_disableInterrupt(MOTOR_PWM,DL_MCPWM_INT_ET_2);
        motor_irq_ack(DL_MCPWM_INT_ET_1);
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        if(last_trip_irq_flags&DL_MCPWM_INT_ET_1)
            (void)am13e_motor_output_zero_irq();
#endif
        return;
    }
#ifdef SW_BLANKING
    const uint32_t events=DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2;
    if(!last_trip_zone_flags && !fault_latched && !(last_trip_irq_flags&~events)){
        motor_irq_ack((uint16_t)last_trip_irq_flags);
        if(last_trip_irq_flags&DL_MCPWM_INT_ET_1)
            (void)am13e_app_motor_bemf_carrier_zero();
        if(last_trip_irq_flags&(DL_MCPWM_INT_ET_1|DL_MCPWM_INT_ET_2))
            am13e_app_motor_bemf_pwm_event();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        if(last_trip_irq_flags&DL_MCPWM_INT_ET_1)
            (void)am13e_motor_output_zero_irq();
#endif
        return;
    }
#endif
    /* Asynchronous TZ overrides ET startup even when both are pending. */
    if(last_trip_zone_flags==0U && last_trip_irq_flags==DL_MCPWM_INT_ET_1 && !fault_latched){
        /* Acknowledge before starting the driver: a later OST keeps its
         * own global/pending interrupt and is not accidentally consumed. */
        motor_irq_ack(DL_MCPWM_INT_ET_1);
        (void)am13e_app_motor_bemf_carrier_zero();
#if AM13E_MOTOR_OUTPUT_CONFIGURED
        if(am13e_motor_output_zero_irq())return;
#endif
        return;
    }
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    if(last_trip_zone_flags==0U && (last_trip_irq_flags&~DL_MCPWM_INT_ET_1)==0U &&
       !motor_timebase_running && !(MOTOR_PWM->INTEN&DL_MCPWM_INT_ET_1)){
        /* A stop may have cleared the source after NVIC latched it. */
        motor_irq_ack(DL_MCPWM_INT_ET_1);
        return;
    }
#endif
    motor_irq_ack((uint16_t)last_trip_irq_flags);
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
}

void am13e_app_motor_fault_reset(void)
{
    /* Rel17 hard_fault_handler expects a nonreturning shutdown/reset.
     * The unconfigured Reference leaves an inspectable latched fault.
     * A configured board requires externally reset-inactive gate wiring.
     */
    __disable_irq();
    fault_latched = 1U;
#if AM13E_MOTOR_OUTPUT_CONFIGURED
    /* Also safe for direct callers, without recursive retries when the
     * pad backend itself failed during the already-frozen shutdown. */
    if(!fault_shutdown_in_progress)am13e_app_motor_fault_shutdown();
    /* Original hard_fault_handler resets after watchdog shutdown. The
     * externally reset-inactive gate circuit is a required board input.
     * Never clear OST or feed the watchdog from this nonreturning path. */
    (void)am13e_app_io_watchdog_fault_arm();
#endif
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
