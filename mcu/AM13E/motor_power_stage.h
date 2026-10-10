/* AM13E Reference PB13 Gate Enable — IO ONLY.
 *
 * PB13 is initialized as GPIO Output at the configured INACTIVE level.
 * Neither this module nor the Rel17 runtime may activate that line.
 * There is no gate driver control, nFAULT Trip, OC Trip or gate attach.
 */
#pragma once
#ifndef AM13E
#error "AM13E reference gate initialization only"
#endif
void am13e_power_stage_init(void);
void am13e_power_stage_force_off(void);
