/* Compile the unmodified shared boot/src/main.c protocol dispatcher
 * into the native host harness under a non-host-main symbol.
 * Production Boot still compiles boot/src/main.c directly.
 */
#define main boot_am13e_protocol_entry
#include "../src/main.c"
