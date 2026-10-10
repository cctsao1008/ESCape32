/* Native Host harness compiles the actual AM13E Rel17 Boot dispatcher.
 * The standalone ARM entry.c is not linked into Host CTest, so rename
 * only the dispatcher function, never the Host's int main(void).
 * The Flash write() symbol is substituted to avoid the POSIX libc ABI.
 */
#define write boot_am13e_flash_write
#define am13e_rel17_boot_main boot_am13e_protocol_entry
#include "../src/main.c"
#undef am13e_rel17_boot_main
#undef write
