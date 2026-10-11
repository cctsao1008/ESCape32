#pragma once
#ifndef AM13E
#error "AM13E auxiliary GPIO only"
#endif
/* PARK_PIN / ERPM_PIN preserve upstream feature selectors. Actual TI
 * pads are mandatory independent board definitions, never legacy ports.
 */
void am13e_app_aux_gpio_init(void);
#ifdef PARK_PIN
int am13e_app_park_level(void);
#endif
#ifdef ERPM_PIN
void am13e_app_erpm_write(int high);
#endif
