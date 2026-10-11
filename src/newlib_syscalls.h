/* Explicit bare-metal Newlib I/O boundary.
 *
 * FW1 PB14 is PWM/DShot, never a UART/stdio console. The Bootloader
 * similarly has no POSIX filesystem. Do not pretend these operations
 * succeeded or introduce semihosting. Return genuine errno failures.
 *
 * Only functions required by linked Newlib are defined. The remaining
 * system calls are provided by the selected standard runtime/specs.
 */
#ifndef AM13E_NEWLIB_SYSCALLS_H
#define AM13E_NEWLIB_SYSCALLS_H
#include <sys/types.h>
int _close(int fd);
off_t _lseek(int fd, off_t offset, int whence);
int _read(int fd, char *data, int length);
int _write(int fd, const char *data, int length);
#endif
