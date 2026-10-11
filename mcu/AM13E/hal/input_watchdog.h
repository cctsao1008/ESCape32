/* AM13E23019 WWDT0 command-link supervisor.
 *
 * Rel17 starts independent watchdog at receiver selection, before CRC.
 * Startup prepares power without starting the counter. Native capture/UART
 * selection arms it; only accepted original Core frames restart it.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E WWDT0 input supervisor only"
#endif
#include <stdint.h>
void am13e_app_io_watchdog_prepare(void);
void am13e_app_io_watchdog_status(uint32_t *armed, uint32_t *feeds);

/* Post-shutdown fault reset: may initialize an otherwise unprepared WWDT.
 * Returns zero on register failure; never feeds or recurses into Core. */
int am13e_app_io_watchdog_fault_arm(void);
