/* E62 MCPWM0 power-stage lifecycle (no motor algorithm).
 *
 * Default firmware: PB13 untouched, six PWM pads GPIO INPUT, force LOW.
 * Optional board profile: explicit PB13 polarity, six pad inversions,
 * independent OC input and proven gate-input/Trip behavior. No defaults
 * may be used to infer unknown electrical parameters.
 *
 * This module is shared by Six-step, Sine, Brushed, Braking and Motor Audio.
 * It does not test hardware or authorize an unreviewed board.
 */
#pragma once
#ifndef AM13E
#error "AM13E power-stage backend only"
#endif
int am13e_power_stage_board_profile_present(void);
void am13e_power_stage_init(void);
void am13e_power_stage_force_off(void);
int am13e_power_stage_attach(void);
int am13e_power_stage_attached(void);
int am13e_power_stage_oc_trip_ready(void);
