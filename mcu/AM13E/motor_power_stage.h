/* AM13E reference MCPWM0 power-stage lifecycle (no motor algorithm).
 *
 * Default FW1: six PWM pads and PB13 connect when Rel17 requests motor
 * or audio, after mandatory PB15 OST1 check. STOP/fault restores gate
 * inactive and pads isolated. Independent OC/OST2 is optional when wired.
 * Numeric G431-derived settings are not electrical qualification.
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
