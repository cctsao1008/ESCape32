/*
 * AM13E / ESCape32 Rel17 Application motor-control boundary (E1-B).
 *
 * No implementations are provided at this stage. An Application firmware
 * MUST NOT link until the platform supplies real, board-qualified drivers.
 *
 * CONTROL SEMANTICS:
 * - AM13E commutation intervals and zero-cross timestamps are logical
 *   microseconds. The backend converts device timer ticks -> microseconds.
 * - phase masks: bit 0=A, 1=B, 2=C. No physical polarity or pin assumption.
 * - PWM duty: Rel17 logical 0..2000 range (not a TI compare register value).
 * - Hardware backend is responsible for break/dead-time, shoot-through
 *   protection, comparator routing, interrupt priorities and fail-safe state.
 * - No power-stage output may be enabled before hardware qualification.
 *
 * Exact timing/behavior must be verified against Rel17 before motor testing.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E motor backend declarations are not for legacy targets"
#endif

void am13e_app_motor_init(void);
void am13e_app_motor_sine_schedule_us(int period_us);
void am13e_app_motor_sine_write(int a, int b, int c, int power, int start);
void am13e_app_motor_sine_finish(void);
void am13e_app_motor_commutation_commit(void);
void am13e_app_motor_sixstep_write(int positive_mask, int negative_mask,
                                   int floating_phase, int damp, int reverse);
void am13e_app_motor_bemf_interval_select(int ertm_us);
void am13e_app_motor_bemf_start(void);
void am13e_app_motor_bemf_stop(void);
void am13e_app_motor_bemf_commutation_delay_us(int delay_us);
void am13e_app_motor_bemf_sine_exit_us(int delay_us);
void am13e_app_motor_sixstep_idle(void);
void am13e_app_motor_pwm_apply(int duty, int freq_min_khz, int freq_max_khz,
                                int ertm_us, int damp, int lock, int brushed);
void am13e_app_motor_brushed_write(int reverse, int damp);
void am13e_app_motor_commutation_enable(int enable);
void am13e_app_motor_fault_shutdown(void);
void am13e_app_motor_fault_reset(void);
void am13e_app_motor_runtime_tick_init(void);
/* Logical reset flags. Backend must map its device reset cause, and
 * distinguish Rel17's reset-induced arming from music suppression.
 * WATCHDOG covers any watchdog reset; FORCE_ARM indicates a reset
 * requiring re-arming (legacy WWDG behavior).
 */
#define AM13E_APP_RESET_WATCHDOG 0x01
#define AM13E_APP_RESET_FORCE_ARM 0x02
int am13e_app_motor_reset_flags(void);
void am13e_app_motor_arming_window_start(void);
int am13e_app_motor_arming_window_expired(void);
void am13e_app_motor_arming_window_restart(void);
void am13e_app_motor_arming_window_stop(void);
void am13e_app_motor_arming_watchdog_refresh(void);

/* Driver dispatches hardware-validated events into shared Rel17 logic. */
void am13e_app_motor_on_commutation_event(void);
void am13e_app_motor_on_bemf_event(int capture_us, int timeout);
