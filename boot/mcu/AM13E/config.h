/* AM13E boot platform configuration.
 * The generic MCU target does not supply a board clock, Flash partition,
 * transport protocol identifier or unqualified pinmux assumptions.
 * Those are owned by the validated boot backend.
 */
#pragma once
#ifndef AM13E
#error "AM13E boot config selected without AM13E"
#endif
#ifndef IO_PB14
#error "AM13E boot target requires IO_PB14"
#endif
