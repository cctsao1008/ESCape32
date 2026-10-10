/* Deliberately unsupported POSIX descriptors on E62 AM13E.
 * Newlib's nosys implementations emit linker warnings and may give the
 * impression that firmware has a UART console. We report a real error
 * instead of masking warnings with -Wno-* or pretending I/O works.
 * PB14 remains exclusively PWM / DShot / BiDShot.
 */
#include "newlib_syscalls.h"
#include <errno.h>

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

off_t _lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return (off_t)-1;
}

int _read(int fd, char *data, int length)
{
    (void)fd;
    (void)data;
    (void)length;
    errno = ENOSYS;
    return -1;
}

int _write(int fd, const char *data, int length)
{
    (void)fd;
    (void)data;
    (void)length;
    errno = ENOSYS;
    return -1;
}
