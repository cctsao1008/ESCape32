/* Compile the unmodified shared boot/src/main.c protocol dispatcher
 * into the native host harness under a non-host-main symbol.
 * Production Boot still compiles boot/src/main.c directly.
 */
/* Host only: redirect the original Boot write() hook to its identical
 * AM13E_FLASH_TEST symbol; avoid interposing libc/POSIX write().
 */
#define write boot_am13e_flash_write
#define main boot_am13e_protocol_entry
#include "../src/main.c"
#undef write
