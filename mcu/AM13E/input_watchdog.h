/* AM13E23019 WWDT0 command-link supervisor.
 *
 * Rel17 starts independent watchdog after accepted receiver activity.
 * Pre-initialize WWDT power without starting the counter; after a valid
 * PWM/DShot packet the original src/io.c callback starts then feeds it.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E WWDT0 input supervisor only"
#endif
#include <stdint.h>
void am13e_app_io_watchdog_prepare(void);
void am13e_app_io_watchdog_status(uint32_t *armed, uint32_t *feeds);
